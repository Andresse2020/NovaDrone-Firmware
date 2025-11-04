/**
 * @file control_motor.c
 * @brief Sensorless BLDC six-step control with SYNCHRONOUS HANDOVER (pro-grade)
 *
 * Key points:
 *  - No torque gap during Open→Closed Loop transition
 *  - Uses real ZC timestamp from BEMF monitor (µs precision)
 *  - Clean, deterministic commutation scheduling
 */

#include "control.h"
#include "service_generic.h"
#include "service_bemf_monitor.h"
#include "service_bldc_motor.h"
#include "service_loop.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ============================================================================
 * Configuration constants
 * ========================================================================== */

#define COMM_DELAY_MIN_US           80.0f
#define COMM_DELAY_MAX_US           30000.0f
#define COMM_LEAD_FACTOR            0.45f       // 27° elec lead angle
#define CL_MIN_VALID_ZC             4           // N consecutive valid ZC for handover
#define CL_MIN_DUTY_TRANSITION      0.20f       // Min duty at transition
#define CL_ENTER_SPEED_HZ           300.0f      // Min speed (Hz elec) for CL entry

/* ============================================================================
 * Motor mode state machine
 * ========================================================================== */

typedef enum {
    MOTOR_MODE_STOPPED = 0,
    MOTOR_MODE_OPEN_LOOP,
    MOTOR_MODE_CLOSED_LOOP
} motor_mode_t;

static motor_mode_t s_motor_mode = MOTOR_MODE_STOPPED;

/* ============================================================================
 * Local control context
 * ========================================================================== */

static struct {
    uint8_t step;
    bool direction_cw;
    float duty;
    bool comm_armed;
    bool transition_scheduled;
    bool handover_armed;
} s_ctx = {
    .step = 0,
    .direction_cw = true,
    .duty = 0.30f,
    .comm_armed = false,
    .transition_scheduled = false,
    .handover_armed = false
};

static s_motor_phase_t s_floating_phase = S_MOTOR_PHASE_A;
static bemf_status_t s_bemf_status;

/* Debug counters */
static uint32_t s_zc_count = 0;
static uint32_t s_comm_count = 0;
static uint32_t s_valid_zc_count = 0;

/* ============================================================================
 * Helpers
 * ========================================================================== */

static s_motor_phase_t Motor_GetFloatingPhase(uint8_t step, bool direction_cw)
{
    /* === Phase flottante par step (doit correspondre aux STATE_HIZ des tables) === */

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

/* ============================================================================
 * Closed-loop commutation (normal operation)
 * ========================================================================== */

static void Motor_ClosedLoop_Commutate(void *user_ctx)
{
    (void)user_ctx;

    s_ctx.comm_armed = false;
    s_ctx.step = (s_ctx.step + 1) % 6;

    Inverter_SixStepCommutate(s_ctx.step, s_ctx.duty, s_ctx.direction_cw);
    s_floating_phase = Motor_GetFloatingPhase(s_ctx.step, s_ctx.direction_cw);

    s_comm_count++;
}

/* ============================================================================
 * Transition commutation (handover event)
 * ========================================================================== */

static void Motor_Transition_Commutate(void *user_ctx)
{
    (void)user_ctx;

    /* Prevent open-loop overlap */
    s_ctx.handover_armed = false;
    s_ctx.transition_scheduled = false;

    /* Perform the commutation */
    s_ctx.step = (s_ctx.step + 1) % 6;
    Inverter_SixStepCommutate(s_ctx.step, s_ctx.duty, s_ctx.direction_cw);
    s_floating_phase = Motor_GetFloatingPhase(s_ctx.step, s_ctx.direction_cw);

    /* Switch to closed-loop mode */
    s_motor_mode = MOTOR_MODE_CLOSED_LOOP;
    s_ctx.comm_armed = false;

    /* Stop open-loop ramp AFTER the first CL commutation */
    Service_Motor_OpenLoopRamp_StopSoft();

    s_comm_count++;

    LOG_INFO("CL handover complete: step=%d float=%d", s_ctx.step, s_floating_phase);

    /* Arm first CL commutation immediately for continuous rotation */
    if (s_bemf_status.valid && !s_ctx.comm_armed) {
        float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;
        if (delay_us < COMM_DELAY_MIN_US) delay_us = COMM_DELAY_MIN_US;
        if (delay_us > COMM_DELAY_MAX_US) delay_us = COMM_DELAY_MAX_US;
        Service_ScheduleCommutation((uint32_t)delay_us, Motor_ClosedLoop_Commutate, NULL);
        s_ctx.comm_armed = true;
    }
}

/* ============================================================================
 * Motor fast loop (24 kHz)
 * ========================================================================== */

void Motor_FastLoop(void)
{
    if (s_motor_mode == MOTOR_MODE_OPEN_LOOP) {
        uint8_t step;
        bool dir;
        Service_Motor_OpenLoopRamp_GetState(&step, NULL, &dir);
        s_floating_phase = Motor_GetFloatingPhase(step, dir);
    }

    SBemfMonitor->process(s_floating_phase);
    SBemfMonitor->get_status(&s_bemf_status);

    if (s_bemf_status.zero_cross_detected)
    {
        s_zc_count++;
        uint32_t now_us = Service_GetTimeUs();

        /* === CLOSED LOOP === */
        if (s_motor_mode == MOTOR_MODE_CLOSED_LOOP && s_bemf_status.valid) {
            if (s_bemf_status.floating_phase == s_floating_phase && !s_ctx.comm_armed) {
                float delay_us = s_bemf_status.period_us * COMM_LEAD_FACTOR;
                if (delay_us < COMM_DELAY_MIN_US) delay_us = COMM_DELAY_MIN_US;
                if (delay_us > COMM_DELAY_MAX_US) delay_us = COMM_DELAY_MAX_US;

                Service_ScheduleCommutation(delay_us, Motor_ClosedLoop_Commutate, NULL);
                s_ctx.comm_armed = true;
            }
        }

        /* === TRANSITION (SYNCHRONOUS HANDOVER) === */
        if (s_motor_mode == MOTOR_MODE_OPEN_LOOP && s_bemf_status.valid && !s_ctx.transition_scheduled)
        {
            float current_speed_hz = 1000000.0f / (6.0f * s_bemf_status.period_us);
            if (current_speed_hz >= CL_ENTER_SPEED_HZ)
            {
                if (s_bemf_status.floating_phase == s_floating_phase)
                    s_valid_zc_count++;
                else
                    s_valid_zc_count = 0;

                if (s_valid_zc_count >= CL_MIN_VALID_ZC)
                {
                    /* Capture state from OL ramp */
                    Service_Motor_OpenLoopRamp_GetState(&s_ctx.step, &s_ctx.duty, &s_ctx.direction_cw);
                    if (s_ctx.duty < CL_MIN_DUTY_TRANSITION)
                        s_ctx.duty = CL_MIN_DUTY_TRANSITION;

                    uint32_t last_zc_time_us = SBemfMonitor->get_last_zc_time_us();
                    float age_us = (float)(now_us - last_zc_time_us);
                    float t_comm_us = (s_bemf_status.period_us * COMM_LEAD_FACTOR) - age_us;

                    if (t_comm_us < COMM_DELAY_MIN_US) {
                        /* Immediate commutation (too late in sector) */
                        Motor_Transition_Commutate(NULL);
                    } else {
                        Service_ScheduleCommutation((uint32_t)t_comm_us, Motor_Transition_Commutate, NULL);
                        s_ctx.transition_scheduled = true;
                        s_ctx.handover_armed = true;
                    }

                    s_valid_zc_count = 0;
                }
            }
        }

        SBemfMonitor->clear_flag();
    }
}

/* ============================================================================
 * Debug telemetry
 * ========================================================================== */

void Control_Motor_PrintStats(void)
{
    char buf_period[16];
    Service_FloatToString(s_bemf_status.period_us, buf_period, 2);
    LOG_INFO("Stats: mode=%d | ZC=%lu | Comm=%lu | step=%d | period=%s us",
             s_motor_mode, s_zc_count, s_comm_count, s_ctx.step, buf_period);
}

/* ============================================================================
 * Start ramp (open-loop)
 * ========================================================================== */

void Control_Motor_StartRamp(void)
{
    SBemfMonitor->reset();
    s_motor_mode = MOTOR_MODE_OPEN_LOOP;
    s_zc_count = s_comm_count = s_valid_zc_count = 0;
    s_ctx.transition_scheduled = false;
    s_ctx.handover_armed = false;
    s_ctx.comm_armed = false;

    LOG_INFO("Starting open-loop ramp");

    Service_Motor_OpenLoopRamp_Start(
        0.3f,   // Start duty
        0.35f,   // End duty
        10.0f,  // Start freq
        500.0f, // End freq
        5000,   // Duration (ms)
        false,   // CCW
        RAMP_PROFILE_LINEAR,
        NULL,
        NULL
    );
}

/* ============================================================================
 * Initialization
 * ========================================================================== */

void Control_Motor_Init(void)
{
    SBemfMonitor->init();
    SFastLoop->init();
    SFastLoop->register_callback(Motor_FastLoop);
    SFastLoop->start();

    LOG_INFO("BEMF Monitor initialized (24 kHz fast loop)");
    Service_Motor_Align_Rotor(0.10f, 500, Control_Motor_StartRamp);
}
