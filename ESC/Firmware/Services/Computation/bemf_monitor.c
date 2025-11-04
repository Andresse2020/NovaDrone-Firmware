/**
 * @file service_bemf_monitor.c
 * @brief Back-EMF (BEMF) monitoring service implementation.
 *
 * This module estimates the electrical period and detects zero-crossings (ZC)
 * of the back-EMF signal in a 3-phase sensorless BLDC motor.
 *
 * It operates inside the fast loop (e.g. 24 kHz) and provides the control layer
 * with clean, validated and filtered zero-crossing events for commutation timing.
 *
 * ----------------------------
 * Design Summary:
 * ----------------------------
 * - Computes BEMF = Vphase - Vneutral
 * - Detects zero-crossings per phase (sign change)
 * - Measures period between same-phase ZCs
 * - Applies amplitude and period validation filters
 * - Provides a smoothed (low-pass filtered) period estimate
 * - Works independently for each phase (3x parallel trackers)
 *
 * Layer: Service (S)
 * Dependencies: i_motor_sensor, i_inverter, i_time
 */

#include "service_bemf_monitor.h"
#include "i_motor_sensor.h"
#include "i_inverter.h"
#include "i_time.h"
#include "service_generic.h"

#include <math.h>
#include <string.h>
#include <stdbool.h>

/* ========================================================================== */
/* === Configuration Constants ============================================= */
/* ========================================================================== */

/** Minimum amplitude (in volts) required to consider a valid BEMF signal. */
#define BEMF_MIN_AMPL_V        0.005f     /**< Reject noise below 5 mV. */

/** Valid period bounds (in microseconds) to filter false zero-cross events. */
#define BEMF_MIN_PERIOD_US     100.0f     /**< Reject extremely fast spikes. */
#define BEMF_MAX_PERIOD_US     50000.0f   /**< Reject too-slow events (>50ms). */

/** Number of consecutive valid/invalid detections required to (un)lock. */
#define BEMF_LOCK_COUNT        2          /**< Number of valid ZC to assert lock. */
#define BEMF_UNLOCK_COUNT      5          /**< Number of invalid ZC to unlock. */

/** Low-pass filter coefficient for period smoothing. */
#define BEMF_FILTER_ALPHA      0.2f       /**< α = 0.2 → 20% new, 80% old. */

/* ========================================================================== */
/* === Module State ======================================================== */
/* ========================================================================== */

/** Global BEMF status structure (exported to control layer). */
static bemf_status_t s_bemf_status;

/** Previous BEMF voltage for sign detection, per phase. */
static float s_prev_bemf[PHASE_COUNT] = {0.0f};

/** Timestamp (µs) of the last detected zero-cross. */
static uint32_t s_last_zc_time_us = 0;

/** Filtered (smoothed) period between ZC. */
static float s_last_period_us = 0.0f;

/** Bootstrap flag: true until first valid ZC is seen on each phase. */
static bool s_bootstrap[PHASE_COUNT] = {true, true, true};

/** Counters for lock validation logic. */
static uint8_t s_valid_streak   = 0;  /**< Number of consecutive valid ZC. */
static uint8_t s_invalid_streak = 0;  /**< Number of consecutive invalid ZC. */
static bool    s_locked         = false; /**< True = BEMF signal considered valid. */

/** Service state flag. */
static bool s_initialized = false;

/* ========================================================================== */
/* === Internal Helper Functions ========================================== */
/* ========================================================================== */

/**
 * @brief Convert raw ADC counts (0–4095) to voltage [V].
 */
static inline float adc_to_voltage(uint16_t raw)
{
    return ((float)raw * 3.3f) / 4095.0f;  // Assuming Vref = 3.3 V
}

/**
 * @brief Compute virtual neutral point voltage.
 * 
 * @param va Phase A voltage [V]
 * @param vb Phase B voltage [V]
 * @param vc Phase C voltage [V]
 * @return Neutral voltage Vn = (Va + Vb + Vc) / 3
 */
static inline float compute_neutral(float va, float vb, float vc)
{
    return (va + vb + vc) / 3.0f;
}

/* ========================================================================== */
/* === BEMF Processing Core =============================================== */
/* ========================================================================== */

/**
 * @brief Process one fast-loop iteration for BEMF monitoring.
 *
 * Called at the fast-loop frequency (typically 24 kHz).
 * Each call measures the back-EMF of the *floating* phase (the one not driven
 * in the current six-step commutation) and checks for a zero-cross event.
 *
 * When a zero-cross is detected:
 *  - The time difference since the last ZC of the *same phase* is measured.
 *  - The period is filtered (low-pass).
 *  - A validation state ("locked") is maintained based on signal consistency.
 *
 * @param floating_phase Phase currently not driven (PHASE_A/B/C)
 */
static void BEMF_Process(uint8_t floating_phase)
{
    /* 1. Ensure service is ready */
    if (!s_initialized || IMotor_ADC_Measure == NULL)
        return;

    /* 2. Read ADC measurements from interface */
    motor_measurements_t meas;
    if (!IMotor_ADC_Measure->get_latest_measurements(&meas))
        return;

    /* 3. Convert raw ADC samples to voltages */
    float Va = adc_to_voltage(meas.v_phase_a_raw);
    float Vb = adc_to_voltage(meas.v_phase_b_raw);
    float Vc = adc_to_voltage(meas.v_phase_c_raw);
    float Vn = compute_neutral(Va, Vb, Vc);

    /* 4. Compute back-EMF for the floating phase */
    float bemf = 0.0f;
    switch (floating_phase)
    {
        case PHASE_A: bemf = Va - Vn; break;
        case PHASE_B: bemf = Vb - Vn; break;
        case PHASE_C: bemf = Vc - Vn; break;
        default: return;
    }

    /* 5. Detect zero-crossing: sign change in BEMF */
    bool zc_detected = ((bemf >= 0.0f && s_prev_bemf[floating_phase] < 0.0f) ||
                        (bemf < 0.0f && s_prev_bemf[floating_phase] >= 0.0f));

    if (!zc_detected)
    {
        s_prev_bemf[floating_phase] = bemf;
        return; // No event this cycle
    }

    /* 6. Reject very small oscillations (noise floor) */
    if (fabsf(bemf) < BEMF_MIN_AMPL_V && fabsf(s_prev_bemf[floating_phase]) < BEMF_MIN_AMPL_V)
    {
        s_prev_bemf[floating_phase] = bemf;
        return;
    }

    uint32_t now_us = ITime->get_time_us();

    /* 7. Bootstrap logic — first ZC per phase initializes baseline */
    if (s_bootstrap[floating_phase])
    {
        s_last_zc_time_us = now_us;
        s_bootstrap[floating_phase] = false;

        s_prev_bemf[floating_phase] = bemf;
        s_bemf_status.zero_cross_detected = false;
        s_bemf_status.valid = false;
        return;
    }

    /* 8. Compute elapsed time since last ZC (from ANY phase), corresponding to 60° electrical */
    float period_us = (float)(now_us - s_last_zc_time_us);
    s_last_zc_time_us = now_us;

    /* 9. Validate period range (reject spikes and dropouts) */
    if (period_us < BEMF_MIN_PERIOD_US || period_us > BEMF_MAX_PERIOD_US)
    {
        if (s_invalid_streak < 255) s_invalid_streak++;
        s_valid_streak = 0;

        /* Too many invalids → unlock BEMF */
        if (s_locked && s_invalid_streak >= BEMF_UNLOCK_COUNT)
            s_locked = false;

        s_bemf_status.zero_cross_detected = false;
        s_bemf_status.valid = s_locked;
        s_prev_bemf[floating_phase] = bemf;
        return;
    }

    /* 10. Smooth the measured period with exponential filter */
    if (s_last_period_us == 0.0f)
        s_last_period_us = period_us;
    else
        s_last_period_us = (1.0f - BEMF_FILTER_ALPHA) * s_last_period_us + BEMF_FILTER_ALPHA * period_us;

    /* 11. Lock/unlock logic */
    if (s_valid_streak < 255) s_valid_streak++;
    s_invalid_streak = 0;

    if (!s_locked && s_valid_streak >= BEMF_LOCK_COUNT)
        s_locked = true;

    /* 12. Update shared BEMF status for control layer */
    s_bemf_status.period_us = s_last_period_us;
    s_bemf_status.floating_phase = floating_phase;
    s_bemf_status.zero_cross_detected = true;
    s_bemf_status.valid = s_locked;

    /* 13. Store last BEMF value for next sign check */
    s_prev_bemf[floating_phase] = bemf;
}

/* ========================================================================== */
/* === Service Management ================================================== */
/* ========================================================================== */

/**
 * @brief Initialize the BEMF monitoring service.
 *
 * This must be called once before using the service. It resets all variables,
 * clears filters, and sets bootstrap flags for all phases.
 */
static bool BEMF_Init(void)
{
    memset(&s_bemf_status, 0, sizeof(s_bemf_status));
    memset(s_prev_bemf, 0, sizeof(s_prev_bemf));
    memset(s_bootstrap, true, sizeof(s_bootstrap));

    s_last_period_us = 0.0f;
    s_last_zc_time_us = 0;
    s_valid_streak = 0;
    s_invalid_streak = 0;
    s_locked = false;
    s_initialized = true;

    return true;
}

/**
 * @brief Reset runtime state (used when restarting open-loop).
 *
 * This clears filters and counters but keeps the service initialized.
 */
static void BEMF_Reset(void)
{
    memset(&s_bemf_status, 0, sizeof(s_bemf_status));
    memset(s_prev_bemf, 0, sizeof(s_prev_bemf));
    memset(s_bootstrap, true, sizeof(s_bootstrap));

    s_last_period_us = 0.0f;
    s_last_zc_time_us = 0;
    s_valid_streak = 0;
    s_invalid_streak = 0;
    s_locked = false;

    s_bemf_status.valid = false;
    s_bemf_status.zero_cross_detected = false;
}

/**
 * @brief Retrieve the latest computed BEMF status.
 *
 * @param out Pointer to destination structure.
 */
static void BEMF_GetStatus(bemf_status_t* out)
{
    if (out)
        *out = s_bemf_status;
}

/**
 * @brief Clear the ZC flag after consumption by the control layer.
 */
static void BEMF_ClearFlag(void)
{
    s_bemf_status.zero_cross_detected = false;
}

/**
 * @brief Get timestamp (µs) of the last detected zero-crossing event.
 */
static uint32_t BEMF_GetLastZCTimeUs(void)
{
    return s_last_zc_time_us;
}


/* ========================================================================== */
/* === Global Service Instance ============================================ */
/* ========================================================================== */

/** Static service descriptor instance. */
static s_bemf_monitor_t s_bemf_service = {
    .init                   =    BEMF_Init,
    .reset                  =    BEMF_Reset,
    .process                =    BEMF_Process,
    .get_status             =    BEMF_GetStatus,
    .clear_flag             =    BEMF_ClearFlag,
    .get_last_zc_time_us    =    BEMF_GetLastZCTimeUs,
};

/** Public pointer to BEMF monitoring service. */
s_bemf_monitor_t* SBemfMonitor = &s_bemf_service;
