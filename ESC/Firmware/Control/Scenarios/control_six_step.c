/**
 * @file control_motor.c
 * @brief Sensorless BLDC six-step motor control with synchronous open→closed loop handover.
 *
 * Design principles:
 *  - Smooth transition (no torque gap)
 *  - µs-accurate commutation scheduling
 *  - Layered structure separating control logic, services, and user API
 *
 * Author: Andresse Bassinga
 */

#include "control_six_step.h"
#include "service_generic.h"
#include "service_bemf_monitor.h"
#include "service_bldc_motor.h"
#include "service_loop.h"
#include "service_pid.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 *  CONFIGURATION CONSTANTS
 * ========================================================================== */

#define MOTOR_POLE_PAIRS             6           ///< Motor pole-pair count (mechanical↔electrical conversion)
#define COMM_DELAY_MIN_US            80.0f       ///< Minimum commutation scheduling delay
#define COMM_DELAY_MAX_US            30000.0f    ///< Maximum commutation delay
#define COMM_LEAD_FACTOR             0.45f       ///< Lead advance factor (≈27° electrical)
#define CL_MIN_VALID_ZC              4           ///< Number of valid zero-crossings before handover
#define CL_MIN_DUTY_TRANSITION       0.20f       ///< Minimum duty cycle at closed-loop entry
#define CL_ENTER_SPEED_HZ            200.0f      ///< Minimum electrical speed to enable closed-loop
#define DEFAULT_RAMP_SLOPE_RPM_MS    10.0f        ///< Default ramp slope (RPM/ms)

/* ============================================================================
 *  LOCAL TYPES AND CONTEXT
 * ========================================================================== */

/**
 * @brief Control mode state machine.
 */
typedef enum {
    MOTOR_MODE_STOPPED = 0,
    MOTOR_MODE_OPEN_LOOP,
    MOTOR_MODE_CLOSED_LOOP
} motor_mode_t;

/**
 * @brief Local runtime context.
 */
typedef struct {
    uint8_t step;
    bool direction_cw;
    float duty;
    bool comm_armed;
    bool transition_scheduled;
    bool handover_armed;
} motor_ctx_t;

/* --- Module state --- */
static motor_mode_t      s_motor_mode = MOTOR_MODE_STOPPED;
static motor_ctx_t       s_ctx = { .direction_cw = true, .duty = 0.3f };
static s_motor_phase_t   s_floating_phase = S_MOTOR_PHASE_A;
static bemf_status_t     s_bemf_status;

/* --- Speed control --- */
static float s_measured_speed_rpm = 0.0f;        ///< Actual speed (from BEMF)
static float s_target_speed_rpm   = 0.0f;        ///< Internal ramped target for PID
static float s_commanded_speed_rpm = 0.0f;       ///< External user command (target speed)
static float s_buffer_speed_rpm = 0.0f;          ///< Internal buffer user command (speed)
static float s_ramp_slope_rpm_ms   = DEFAULT_RAMP_SLOPE_RPM_MS;

/* --- PID controller --- */
static pid_t speed_pid;

/* --- Debug counters --- */
static uint32_t s_zc_count = 0;
static uint32_t s_comm_count = 0;
static uint32_t s_valid_zc_count = 0;

/* --- Reverse handling flag --- */
static bool s_reverse_pending = false;

/* ============================================================================
 *  STATIC (INTERNAL) FUNCTIONS
 * ========================================================================== */

/**
 * @brief Return the floating phase for a given step and direction.
 */
static s_motor_phase_t Motor_GetFloatingPhase(uint8_t step, bool direction_cw)
{
    /* Phase flottante par step (doit correspondre aux STATE_HIZ des tables) */

    static const s_motor_phase_t float_phase_cw[6] = {
        S_MOTOR_PHASE_C, // Step 0 → C est flottante
        S_MOTOR_PHASE_B, // Step 1 → B est flottante
        S_MOTOR_PHASE_A, // Step 2 → A est flottante
        S_MOTOR_PHASE_C, // Step 3 → C est flottante
        S_MOTOR_PHASE_B, // Step 4 → B est flottante
        S_MOTOR_PHASE_A  // Step 5 → A est flottante
    };

    static const s_motor_phase_t float_phase_ccw[6] = {
        S_MOTOR_PHASE_A, // Step 0 → A est flottante
        S_MOTOR_PHASE_B, // Step 1 → B est flottante
        S_MOTOR_PHASE_C, // Step 2 → C est flottante
        S_MOTOR_PHASE_A, // Step 3 → A est flottante
        S_MOTOR_PHASE_B, // Step 4 → B est flottante
        S_MOTOR_PHASE_C  // Step 5 → C est flottante
    };

    return direction_cw ? float_phase_cw[step % 6] : float_phase_ccw[step % 6];
}

/**
 * @brief Handle closed-loop commutation event.
 */
static void Motor_ClosedLoop_Commutate(void *user_ctx)
{
    (void)user_ctx;

    s_ctx.comm_armed = false;
    s_ctx.step = (s_ctx.step + 1) % 6;

    Inverter_SixStepCommutate(s_ctx.step, s_ctx.duty, s_ctx.direction_cw);
    s_floating_phase = Motor_GetFloatingPhase(s_ctx.step, s_ctx.direction_cw);
    s_comm_count++;
}

/**
 * @brief Handle open→closed loop transition (synchronous handover).
 */
static void Motor_Transition_Commutate(void *user_ctx)
{
    (void)user_ctx;

    /* Prevent overlap with open-loop sequence */
    s_ctx.transition_scheduled = false;
    s_ctx.handover_armed = false;

    /* Perform commutation */
    s_ctx.step = (s_ctx.step + 1) % 6;
    Inverter_SixStepCommutate(s_ctx.step, s_ctx.duty, s_ctx.direction_cw);
    s_floating_phase = Motor_GetFloatingPhase(s_ctx.step, s_ctx.direction_cw);

    /* Switch to closed-loop mode */
    s_motor_mode = MOTOR_MODE_CLOSED_LOOP;
    s_ctx.comm_armed = false;

    /* Stop open-loop ramp gracefully */
    Service_Motor_OpenLoopRamp_StopSoft();

    /* Synchronize ramp with actual speed */
    float electrical_freq_hz = 1e6f / (6.0f * s_bemf_status.period_us);
    s_measured_speed_rpm = (electrical_freq_hz * 60.0f) / MOTOR_POLE_PAIRS;
    s_target_speed_rpm = s_measured_speed_rpm;

    /* Arm first commutation immediately for continuous motion */
    if (s_bemf_status.valid && !s_ctx.comm_armed)
    {
        float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;
        delay_us = fminf(fmaxf(delay_us, COMM_DELAY_MIN_US), COMM_DELAY_MAX_US);
        Service_ScheduleCommutation((uint32_t)delay_us, Motor_ClosedLoop_Commutate, NULL);
        s_ctx.comm_armed = true;
    }
}

/**
 * @brief Motor fast loop (executed at 24 kHz).
 *
 * This function is the high-frequency core of the sensorless control algorithm.
 * It performs:
 *   - Continuous BEMF sampling and zero-crossing detection
 *   - Open→Closed loop synchronization (handover)
 *   - Real-time commutation scheduling
 *
 * The function must be extremely fast and deterministic.
 */
static void Motor_FastLoop(void)
{
    /* ----------------------------------------------------------------------
     * 1. UPDATE FLOATING PHASE IN OPEN-LOOP MODE
     * ----------------------------------------------------------------------
     * In open-loop startup, commutation steps are generated by a precomputed
     * ramp (frequency and duty increase over time). The "floating phase"
     * is the one not driven at each step — needed for BEMF measurement.
     * Here we update it in real-time to keep BEMF monitoring aligned.
     */
    if (s_motor_mode == MOTOR_MODE_OPEN_LOOP)
    {
        uint8_t step;
        bool dir;

        /* Query current step & direction from the open-loop ramp service */
        Service_Motor_OpenLoopRamp_GetState(&step, NULL, &dir);

        /* Determine which motor phase is floating at this step */
        s_floating_phase = Motor_GetFloatingPhase(step, dir);
    }

    /* ----------------------------------------------------------------------
     * 2. PROCESS BEMF SIGNAL
     * ----------------------------------------------------------------------
     * The BEMF monitor samples ADC data for the floating phase,
     * detects zero-cross events (when back-EMF crosses virtual neutral),
     * and computes period & validity of the signal.
     */
    SBemfMonitor->process(s_floating_phase);
    SBemfMonitor->get_status(&s_bemf_status);

    /* If no zero-crossing was detected this cycle, exit early */
    if (!s_bemf_status.zero_cross_detected)
        return;

    s_zc_count++;                                   // Increment ZC counter for debugging
    uint32_t now_us = Service_GetTimeUs();          // Capture current timestamp (µs precision)

    /* ----------------------------------------------------------------------
     * 3. CLOSED-LOOP COMMUTATION (Normal running mode)
     * ----------------------------------------------------------------------
     * Once the motor runs sensorlessly, we schedule each commutation
     * after a precise delay from the detected zero-crossing.
     *
     * The delay = (BEMF period × lead factor), which corresponds
     * to advancing the next phase by ≈27° electrical.
     */
    if (s_motor_mode == MOTOR_MODE_CLOSED_LOOP && s_bemf_status.valid)
    {
        /* Ensure the zero-cross came from the correct floating phase
         * and no commutation is already pending.
         */
        if (s_bemf_status.floating_phase == s_floating_phase && !s_ctx.comm_armed)
        {
            /* Compute commutation delay (lead angle compensation) */
            float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;

            /* Clamp the delay to safe bounds to avoid missed commutation */
            delay_us = fminf(fmaxf(delay_us, COMM_DELAY_MIN_US), COMM_DELAY_MAX_US);

            /* Schedule commutation callback */
            Service_ScheduleCommutation(delay_us, Motor_ClosedLoop_Commutate, NULL);
            s_ctx.comm_armed = true;
        }
    }

    /* ----------------------------------------------------------------------
     * 4. TRANSITION (SYNCHRONOUS HANDOVER)
     * ----------------------------------------------------------------------
     * During startup (open-loop), this logic decides when to switch
     * to sensorless closed-loop control.
     *
     * The handover occurs when:
     *   - Valid BEMF is detected on the correct floating phase
     *   - A minimum number of consecutive valid ZCs are observed
     *   - The electrical speed exceeds CL_ENTER_SPEED_HZ
     */
    if (s_motor_mode == MOTOR_MODE_OPEN_LOOP && s_bemf_status.valid && !s_ctx.transition_scheduled)
    {
        /* Compute current electrical speed from BEMF period */
        float current_speed_hz = 1e6f / (6.0f * s_bemf_status.period_us);

        /* Check if we reached minimum speed for BEMF detection reliability */
        if (current_speed_hz >= CL_ENTER_SPEED_HZ)
        {
            /* Increment valid-ZC counter if same floating phase, else reset */
            s_valid_zc_count = (s_bemf_status.floating_phase == s_floating_phase)? (s_valid_zc_count + 1) : 0;

            /* If N consecutive valid ZCs → ready for handover */
            if (s_valid_zc_count >= CL_MIN_VALID_ZC)
            {
                /* Capture the latest state from open-loop ramp */
                Service_Motor_OpenLoopRamp_GetState(&s_ctx.step, &s_ctx.duty, &s_ctx.direction_cw);

                /* Enforce a minimum duty before switching to CL */
                s_ctx.duty = fmaxf(s_ctx.duty, CL_MIN_DUTY_TRANSITION);

                /* Compute the elapsed time since the last zero-cross */
                float age_us = (float)(now_us - SBemfMonitor->get_last_zc_time_us());

                /* Compute exact commutation time (synchronous handover) */
                float t_comm_us = (s_bemf_status.period_us * COMM_LEAD_FACTOR) - age_us;

                /* If we're already past the ideal point → commutate immediately,
                   else schedule precise transition commutation. */
                if (t_comm_us < COMM_DELAY_MIN_US)
                {
                    Motor_Transition_Commutate(NULL);
                }
                else
                {
                    Service_ScheduleCommutation((uint32_t)t_comm_us, Motor_Transition_Commutate, NULL);
                    s_ctx.transition_scheduled = true;
                    s_ctx.handover_armed = true;
                }

                /* Reset counter for next detection sequence */
                s_valid_zc_count = 0;
            }
        }
    }

    /* ----------------------------------------------------------------------
     * 5. CLEAR FLAGS FOR NEXT ITERATION
     * ----------------------------------------------------------------------
     * Reset zero-crossing detection flag to avoid retriggering
     * on the same event in the next loop cycle.
     */
    SBemfMonitor->clear_flag();
}


/**
 * @brief Start open-loop ramp (alignment already done).
 */
static void Motor_StartOpenLoopRamp(void)
{
    SBemfMonitor->reset();
    s_motor_mode = MOTOR_MODE_OPEN_LOOP;

    s_zc_count = s_comm_count = s_valid_zc_count = 0;
    s_ctx.transition_scheduled = s_ctx.handover_armed = s_ctx.comm_armed = false;

    LOG_INFO("Starting open-loop ramp...");
    Service_Motor_OpenLoopRamp_Start(
        0.5f,    // Start duty
        0.6f,     // End duty
        25.0f,    // Start freq
        500.0f,   // End freq
        1000,     // Duration (ms)
        s_ctx.direction_cw, // CCW/CCW
        RAMP_PROFILE_EXPONENTIAL,
        NULL,
        NULL
    );
}


/**
 * @brief 1 kHz slow loop: speed ramp + PID control.
 */
static void Motor_LowLoop(void)
{
    /* --- Speed measurement (mechanical RPM) --- */
    if (s_bemf_status.valid && s_bemf_status.period_us > 0)
    {
        float f_elec = 1e6f / (6.0f * s_bemf_status.period_us);
        s_measured_speed_rpm = (f_elec * 60.0f) / MOTOR_POLE_PAIRS;
    }

    /* --- Target ramp (active only in closed-loop) --- */
    if (s_motor_mode == MOTOR_MODE_CLOSED_LOOP)
    {
        float delta = s_commanded_speed_rpm - s_target_speed_rpm;
        delta = fminf(fmaxf(delta, -s_ramp_slope_rpm_ms), s_ramp_slope_rpm_ms);
        s_target_speed_rpm += delta;
    }
    else s_target_speed_rpm = 0.0f;

    /* --- PID update --- */
    if (s_motor_mode == MOTOR_MODE_CLOSED_LOOP && s_bemf_status.valid)
    {
        s_ctx.duty = Service_PID_Update(&speed_pid, s_target_speed_rpm, s_measured_speed_rpm);
    }

    /* --- Handle pending reversal --- */
    if (s_reverse_pending && s_measured_speed_rpm < 400.0f)
    {
        s_reverse_pending = false;
        s_ctx.direction_cw = !s_ctx.direction_cw;

        LOG_INFO("Restarting in opposite direction (%s)",
                s_ctx.direction_cw ? "CW" : "CCW");

        Service_Motor_Stop();
        s_motor_mode = MOTOR_MODE_STOPPED;

        /* Apply buffered speed command after reversal */
        s_commanded_speed_rpm = s_buffer_speed_rpm;

        Service_Motor_Align_Rotor(0.10f, 500, Motor_StartOpenLoopRamp);
    }

}


/* ============================================================================
 *  PUBLIC USER-LEVEL API
 * ========================================================================== */

/**
 * @brief Initialize motor control module and register loops.
 */
void Control_Motor_Init(void)
{
    SBemfMonitor->init();

    /* Fast loop (24 kHz) */
    SFastLoop->init();
    SFastLoop->register_callback(Motor_FastLoop);
    SFastLoop->start();

    /* PID configuration (1 kHz) */
    Service_PID_Init(&speed_pid, 0.0005f, 0.001f, 0.0f, 0.001f);
    speed_pid.out_min = 0.05f;
    speed_pid.out_max = 0.95f;
    speed_pid.integrator_limit = 0.5f;

    /* Slow loop (1 kHz) */
    SLowLoop->init();
    SLowLoop->register_callback(Motor_LowLoop);
    SLowLoop->start();

    LOG_INFO("Motor control initialized.");
}

/**
 * @brief Set motor speed command (in RPM, signed).
 *
 * This function manages both speed updates and direction changes.
 * If the requested direction is opposite to the current one,
 * the motor will first decelerate to a stop, then restart safely
 * in the new direction.
 *
 * @param rpm  Target mechanical speed in RPM (signed).
 */
void Control_Motor_SetSpeed_RPM(float rpm)
{
    /* ----------------------------------------------------------------------
     * 1. Determine requested direction and magnitude
     * ---------------------------------------------------------------------- */
    bool new_dir_cw = (rpm >= 0.0f);
    float target_rpm = fabsf(rpm);

    /* ----------------------------------------------------------------------
     * 2. Case: Motor currently stopped → start directly
     * ---------------------------------------------------------------------- */
    if (s_motor_mode == MOTOR_MODE_STOPPED)
    {
        LOG_INFO("Motor start: (%s)", new_dir_cw ? "CW" : "CCW");
        s_ctx.direction_cw = new_dir_cw;
        s_commanded_speed_rpm = target_rpm;
        Service_Motor_Align_Rotor(0.10f, 500, Motor_StartOpenLoopRamp);
        return;
    }

    /* ----------------------------------------------------------------------
     * 3. Case: Motor running in opposite direction → safe reversal sequence
     * ----------------------------------------------------------------------
     * - Step 1: Command 0 RPM (smooth stop via ramp)
     * - Step 2: Wait until measured speed < threshold
     * - Step 3: Restart with opposite direction
     */
    if (s_ctx.direction_cw != new_dir_cw && s_motor_mode != MOTOR_MODE_STOPPED)
    {
        LOG_WARN("Direction change detected: %s → %s. Initiating safe stop...",
                 s_ctx.direction_cw ? "CW" : "CCW",
                 new_dir_cw ? "CW" : "CCW");

        /* --- Start deceleration --- */
        s_commanded_speed_rpm = 0.0f;

        /* --- Buffer new speed after reversal --- */
        s_buffer_speed_rpm = target_rpm;

        /* wait flag to prevent new start before complete stop */
        s_reverse_pending = true;
        return;
    }

    /* ----------------------------------------------------------------------
     * 4. Case: Motor already running in same direction → update target
     * ---------------------------------------------------------------------- */
    if (s_motor_mode == MOTOR_MODE_OPEN_LOOP || s_motor_mode == MOTOR_MODE_CLOSED_LOOP)
    {
        s_commanded_speed_rpm = target_rpm;
        LOG_DEBUG("Speed update: %.0f RPM (%s)", target_rpm, new_dir_cw ? "CW" : "CCW");
    }
}


/**
 * @brief Smoothly stop the motor (soft stop).
 */
void Control_Motor_Stop(void)
{
    s_commanded_speed_rpm = 0.0f;

    Service_Motor_Stop();

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_motor_mode = MOTOR_MODE_STOPPED;
    s_target_speed_rpm = s_measured_speed_rpm = 0.0f;
}

/**
 * @brief Return current commanded speed (RPM).
 */
float Control_Motor_GetTargetSpeed_RPM(void)
{
    return s_measured_speed_rpm < 300.0? 0.0 : s_measured_speed_rpm;
}

/**
 * @brief Set maximum speed ramp slope (RPM per ms).
 */
void Control_Motor_SetRampSlope(float rpm_per_ms)
{
    s_ramp_slope_rpm_ms = fminf(fmaxf(rpm_per_ms, 1.0f), 500.0f);
}

/**
 * @brief Print current motor control status (human-readable telemetry).
 *
 * Displays:
 *  - Whether the motor is active or stopped
 *  - Current mode (STOPPED / OL / CL)
 *  - Direction of rotation (CW / CCW)
 *  - Measured mechanical speed (RPM)
 */
void Control_Motor_PrintStats(void)
{
    /* Determine if motor is active */
    bool is_running = (s_motor_mode != MOTOR_MODE_STOPPED);

    /* Determine textual mode label */
    const char *mode_str =
        (s_motor_mode == MOTOR_MODE_STOPPED)    ? "STOPPED" :
        (s_motor_mode == MOTOR_MODE_OPEN_LOOP)  ? "OPEN_LOOP" :
        (s_motor_mode == MOTOR_MODE_CLOSED_LOOP)? "CLOSED_LOOP" :
                                                  "UNKNOWN";

    /* Determine rotation direction (if relevant) */
    const char *dir_str = s_ctx.direction_cw ? "CW" : "CCW";

    /* Compute human-readable speed */
    float rpm = s_measured_speed_rpm;

    /* Print according to motor state */
    if (!is_running || rpm < 50.0f) // threshold to filter noise
    {
        LOG_INFO("[Motor] Status: \033[31mSTOPPED\033[0m");
    }
    else
    {
        LOG_INFO("[Motor] RUNNING | Mode=%s | Dir=%s | Speed=%lu RPM",
                 mode_str, dir_str, (uint32_t)rpm);
    }
}