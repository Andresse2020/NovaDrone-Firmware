/**
 * @file control_motor.c
 * @brief Example control-layer test for BEMF monitoring service.
 *
 * This function demonstrates how to integrate SBemfMonitor into the 24 kHz
 * fast control loop. It logs zero-cross events using LOG_INFO().
 */

#include "control.h"
#include "services.h"
#include "bemf_monitor.h"
#include "service_loop.h"

/* ========================================================================== */
/* === Local Variables ===================================================== */
/* ========================================================================== */

/** Current floating phase (defined by six-step commutation pattern) */
static s_motor_phase_t s_floating_phase = S_MOTOR_PHASE_A;

/** Structure holding the latest BEMF status */
static bemf_status_t s_bemf_status;

char buffer[16];

/* ========================================================================== */
/* === Control Fast Loop Callback ========================================== */
/* ========================================================================== */

/**
 * @brief Motor fast loop callback (executed at 24 kHz by SFastLoop).
 *
 * This routine periodically processes the back-EMF signal of the floating phase
 * and detects zero-cross events. Each event is logged for observation.
 *
 * In a real sensorless control system, the zero-cross timing would be used
 * to synchronize commutation rather than print debug messages.
 */
void Motor_FastLoop(void)
{
    /* Étape 1 — Process the floating phase BEMF */
    SBemfMonitor->process(s_floating_phase);

    /* Étape 2 — Retrieve the latest BEMF status */
    SBemfMonitor->get_status(&s_bemf_status);

    Service_FloatToString(s_bemf_status.period_us, buffer, 2);

    /* Étape 3 — Check if a zero-cross event occurred */
    if (s_bemf_status.zero_cross_detected)
    {
        /* Log event information */
        LOG_INFO("BEMF ZC detected on phase %d | period = %s us | valid = %d",
                 s_bemf_status.floating_phase,
                 buffer,
                 s_bemf_status.valid);

        /* Clear detection flag for next event */
        SBemfMonitor->clear_flag();
    }

    /* Étape 4 — (Optional)
     * You can update s_floating_phase dynamically according to
     * your commutation step index from the ramp or control logic.
     */
}

/* ========================================================================== */
/* === Initialization Routine ============================================= */
/* ========================================================================== */

void Control_Motor_StartRamp(void)
{
    /* Example: linear open-loop ramp from 5 Hz to 100 Hz over 3 s */
    Service_Motor_OpenLoopRamp_Start(
        0.25f,          // Starting duty
        0.50f,          // Final duty
        5.0f,           // Start frequency (Hz)
        500.0f,         // End frequency (Hz)
        3000,           // Ramp duration (ms)
        true,           // Direction: clockwise
        RAMP_PROFILE_LINEAR,  // Ramp type
        NULL,           // Optional callback when complete
        NULL);          // Optional user context
}

/**
 * @brief Initialize the motor control test environment.
 *
 * This sets up the BEMF monitoring service and starts the 24 kHz fast loop.
 * Call once from the application layer (App) before running any motor tests.
 */
void Control_Motor_Init(void)
{
    /* Initialize BEMF monitoring service */
    SBemfMonitor->init();

    /* Initialize and start the Fast Loop service */
    SFastLoop->init();

    /* Register fast loop callback */
    SFastLoop->register_callback(Motor_FastLoop);

    /* Start periodic fast loop timer */
    SFastLoop->start();

    LOG_INFO("BEMF Monitor test started (24 kHz fast loop)");

    /* Start the open-loop ramp */
    Control_Motor_StartRamp();

    LOG_INFO("Open-loop ramp started");
}
