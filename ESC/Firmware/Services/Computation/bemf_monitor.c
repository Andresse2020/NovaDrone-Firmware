/**
 * @file service_bemf_monitor.c
 * @brief Back-EMF (BEMF) monitoring service implementation.
 *
 * This service estimates the electrical period and detects zero-crossings
 * of the motor back-EMF signal for sensorless BLDC control.
 * It runs at the fast-loop rate (e.g. 24 kHz) and provides filtered,
 * non-blocking BEMF data to the Control layer.
 *
 * Layer: Service (S)
 * Dependencies: Interface (i_motor_sensor, i_inverter, i_time)
 */

#include "bemf_monitor.h"
#include "i_motor_sensor.h"   // Provides IMotor_ADC_Measure for phase voltage sampling
#include "i_inverter.h"       // Provides PHASE_A/B/C identifiers for low-level conversion
#include "i_time.h"           // Provides ITime->get_time_us() for time measurement
#include <math.h>
#include <string.h>

/* ========================================================================== */
/* === Static Variables ==================================================== */
/* ========================================================================== */

/* ==== Module-level persistent state (place these at file scope) ========= */
static uint8_t s_valid_streak   = 0;     // count of consecutive valid ZC
static uint8_t s_invalid_streak = 0;     // count of consecutive invalid ZC
static bool    s_locked         = false; // validation state
static float   s_last_period_us = 0.0f;  // filtered period for display
static bool    s_bootstrap      = true;  // ignore first ZC period (baseline only)

/* ==== Tunable thresholds (calibrated for slow startup) ================== */
#define BEMF_MIN_AMPL_V        0.005f      /* ~2 mV, permissive for startup   */
#define BEMF_MIN_PERIOD_US     100.0f      /* reject extremely fast spikes     */
#define BEMF_MAX_PERIOD_US     300000.0f   /* allow up to 300 ms (very slow)   */
#define BEMF_LOCK_COUNT        2           /* #valid ZC before valid=1         */
#define BEMF_UNLOCK_COUNT      5           /* #invalid ZC before valid=0       */

/** Current filtered BEMF status structure */
static bemf_status_t s_bemf_status;

/** Previous computed BEMF value (for zero-cross detection) */
static float s_prev_bemf = 0.0f;

/** Timestamp (in µs) of the last detected zero-cross */
static uint32_t s_last_zc_time_us = 0;

/** True if the service has been initialized */
static bool s_initialized = false;

/* ========================================================================== */
/* === Internal Helpers ==================================================== */
/* ========================================================================== */

/**
 * @brief Convert raw ADC counts (0–4095) to voltage in volts.
 * @param raw Raw ADC sample (0–4095)
 * @return Converted voltage [V]
 */
static inline float adc_to_voltage(uint16_t raw)
{
    return ((float)raw * 3.3f) / 4095.0f;  // Adjust if Vref != 3.3V
}

/**
 * @brief Compute the virtual neutral voltage based on three phase voltages.
 * @param va Phase A voltage [V]
 * @param vb Phase B voltage [V]
 * @param vc Phase C voltage [V]
 * @return Computed neutral point voltage [V]
 */
static inline float compute_neutral(float va, float vb, float vc)
{
    return (va + vb + vc) / 3.0f;
}

/**
 * @brief Convert service-level motor phase to low-level inverter phase.
 * @param s_phase Phase identifier at service level
 * @return Corresponding hardware phase for inverter driver
 */
static inline inverter_phase_t sphase_to_hwphase(s_motor_phase_t s_phase)
{
    switch (s_phase)
    {
        case S_MOTOR_PHASE_A: return PHASE_A;
        case S_MOTOR_PHASE_B: return PHASE_B;
        case S_MOTOR_PHASE_C: return PHASE_C;
        default:              return PHASE_A;  // Safe fallback
    }
}

/**
 * @brief Convert hardware-level inverter phase to service-level phase.
 * @param hw_phase Inverter driver phase identifier
 * @return Corresponding service-level phase identifier
 */
static inline s_motor_phase_t hwphase_to_sphase(inverter_phase_t hw_phase)
{
    switch (hw_phase)
    {
        case PHASE_A: return S_MOTOR_PHASE_A;
        case PHASE_B: return S_MOTOR_PHASE_B;
        case PHASE_C: return S_MOTOR_PHASE_C;
        default:      return S_MOTOR_PHASE_A;
    }
}

/* ========================================================================== */
/* === Public Functions ==================================================== */
/* ========================================================================== */

/**
 * @brief Initialize the BEMF monitoring service.
 *
 * This function resets all internal variables, timestamps, and flags.
 * It must be called once before using the service.
 *
 * @retval true Initialization successful
 * @retval false Should never occur (reserved for future checks)
 */
static bool BEMF_Init(void)
{
    memset(&s_bemf_status, 0, sizeof(s_bemf_status));
    s_prev_bemf = 0.0f;
    s_last_zc_time_us = ITime->get_time_us();
    s_initialized = true;
    return true;
}

/**
 * @brief Process one new ADC sample and update the BEMF state.
 *
 * This function must be called at the fast-loop frequency (e.g. 24 kHz).
 * It reads the latest phase voltages from the ADC interface, computes
 * the virtual neutral point, and evaluates the floating phase BEMF.
 *
 * When a zero-cross is detected (sign change of BEMF), the function:
 *  - Calculates the electrical period (µs),
 *  - Applies low-pass filtering for stability,
 *  - Flags the event for the Control layer.
 *
 * @param s_phase Floating phase currently not driven by PWM
 */
static void BEMF_Process(uint8_t floating_phase)
{
    if (!s_initialized || IMotor_ADC_Measure == NULL)
        return;

    motor_measurements_t meas;
    if (!IMotor_ADC_Measure->get_latest_measurements(&meas))
        return;

    /* Convert raw ADC to voltages */
    float Va = adc_to_voltage(meas.v_phase_a_raw);
    float Vb = adc_to_voltage(meas.v_phase_b_raw);
    float Vc = adc_to_voltage(meas.v_phase_c_raw);
    float Vn = (Va + Vb + Vc) / 3.0f;

    /* Compute BEMF on floating phase */
    float bemf = 0.0f;
    switch (floating_phase)
    {
        case 0: bemf = Va - Vn; break;  // PHASE_A
        case 1: bemf = Vb - Vn; break;  // PHASE_B
        case 2: bemf = Vc - Vn; break;  // PHASE_C
        default: return;
    }

    /* Zero-cross detection (sign change) */
    bool zc_detected = ((bemf >= 0.0f && s_prev_bemf < 0.0f) ||
                        (bemf < 0.0f && s_prev_bemf >= 0.0f));

    if (zc_detected)
    {
        /* 1) Amplitude guard: reject tiny signals (noise) */
        if (fabsf(bemf) < BEMF_MIN_AMPL_V && fabsf(s_prev_bemf) < BEMF_MIN_AMPL_V)
        {
            s_prev_bemf = bemf;
            return;
        }

        uint32_t now_us = ITime->get_time_us();

        /* 2) Bootstrap: first ZC only sets the baseline, no period computation */
        if (s_bootstrap)
        {
            s_last_zc_time_us = now_us;       // establish baseline
            s_bootstrap = false;
            s_valid_streak = 0;
            s_invalid_streak = 0;
            s_bemf_status.zero_cross_detected = false; // no event to consume
            s_bemf_status.valid = false;
            s_prev_bemf = bemf;
            return;
        }

        /* 3) Compute raw period since last accepted ZC */
        float period_us = (float)(now_us - s_last_zc_time_us);

        /* 4) Check raw period bounds */
        if (period_us < BEMF_MIN_PERIOD_US || period_us > BEMF_MAX_PERIOD_US)
        {
            /* invalid ZC: increase invalid streak, maybe unlock */
            if (s_invalid_streak < 255) s_invalid_streak++;
            s_valid_streak = 0;

            if (s_locked && s_invalid_streak >= BEMF_UNLOCK_COUNT)
                s_locked = false;

            s_bemf_status.zero_cross_detected = false;
            s_bemf_status.valid = s_locked;
            s_prev_bemf = bemf;
            return;
        }

        /* 5) Accept this ZC: update timestamp baseline */
        s_last_zc_time_us = now_us;

        /* 6) Reset invalid streak and update filtered period */
        s_invalid_streak = 0;
        if (s_last_period_us == 0.0f)
            s_last_period_us = period_us;
        else
            s_last_period_us = 0.9f * s_last_period_us + 0.1f * period_us;

        s_bemf_status.period_us = s_last_period_us;
        s_bemf_status.floating_phase = floating_phase;
        s_bemf_status.zero_cross_detected = true;

        /* 7) Lock-in logic */
        if (s_valid_streak < 255) s_valid_streak++;
        if (!s_locked && s_valid_streak >= BEMF_LOCK_COUNT)
            s_locked = true;

        s_bemf_status.valid = s_locked;
    }

    /* Remember for next sign-change test */
    s_prev_bemf = bemf;
}

/**
 * @brief Retrieve the latest computed BEMF status.
 * @param out Pointer to destination structure
 */
static void BEMF_GetStatus(bemf_status_t* out)
{
    if (out)
        *out = s_bemf_status;
}

/**
 * @brief Clear the zero-cross flag after the event has been processed.
 *
 * This prevents the Control layer from reading the same event multiple times.
 */
static void BEMF_ClearFlag(void)
{
    s_bemf_status.zero_cross_detected = false;
}

/**
 * @brief Reset all BEMF internal states without reinitializing hardware.
 *
 * This function is used when restarting a new open-loop ramp,
 * to clear previous detection history and ensure a clean start.
 *
 * It preserves initialization and hardware configuration,
 * but clears dynamic runtime variables such as lock state, period,
 * and previous BEMF value.
 */
static void BEMF_Reset(void)
{
    memset(&s_bemf_status, 0, sizeof(s_bemf_status));

    s_prev_bemf = 0.0f;
    s_last_period_us = 0.0f;
    s_last_zc_time_us = ITime->get_time_us();

    s_valid_streak = 0;
    s_invalid_streak = 0;
    s_locked = false;
    s_bootstrap = true;  // Force first ZC to establish baseline only

    s_bemf_status.valid = false;
    s_bemf_status.zero_cross_detected = false;
}


/* ========================================================================== */
/* === Global Service Interface ============================================ */
/* ========================================================================== */

/**
 * @brief Static instance of the BEMF service function table.
 *
 * This structure implements the service-level API defined in
 * service_bemf_monitor.h and is exposed through the SBemfMonitor pointer.
 */
static s_bemf_monitor_t s_bemf_service = {
    .init       = BEMF_Init,
    .reset      = BEMF_Reset,
    .process    = BEMF_Process,
    .get_status = BEMF_GetStatus,
    .clear_flag = BEMF_ClearFlag,
};

/**
 * @brief Global pointer to the BEMF monitoring service instance.
 *
 * Example usage (Control layer):
 * @code
 *     SBemfMonitor->init();
 *     SBemfMonitor->process(S_MOTOR_PHASE_A);
 *     bemf_status_t status;
 *     SBemfMonitor->get_status(&status);
 * @endcode
 */
s_bemf_monitor_t* SBemfMonitor = &s_bemf_service;