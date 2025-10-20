#ifndef SERVICE_BEMF_MONITOR_H
#define SERVICE_BEMF_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 *  SERVICE: BEMF MONITOR
 *  Layer: Service (S)
 *  Description:
 *      This service provides zero-cross detection and electrical period
 *      estimation from phase back-EMF voltages. It acts as a mid-layer
 *      abstraction, independent of low-level ADC or inverter driver details.
 * ========================================================================== */

/**
 * @brief Logical representation of motor phases at the service level.
 *
 * This enum is independent of hardware-specific inverter types
 * and should be used exclusively in the Control layer.
 */
typedef enum
{
    S_MOTOR_PHASE_A = 0,   /**< Phase A */
    S_MOTOR_PHASE_B,       /**< Phase B */
    S_MOTOR_PHASE_C,       /**< Phase C */
    S_MOTOR_PHASE_COUNT
} s_motor_phase_t;

/**
 * @brief Status of the BEMF monitor.
 *
 * Contains zero-cross detection results and timing information.
 */
typedef struct
{
    bool zero_cross_detected;        /**< True when a new zero-cross is detected */
    float period_us;                 /**< Electrical period (µs) between two ZC */
    s_motor_phase_t floating_phase;  /**< Current floating phase */
    bool valid;                      /**< True when period is stable (filtered) */
} bemf_status_t;

/* ---------------------------------------------------------------------------
 * Function Pointer Types
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize BEMF monitoring.
 * @return true on success, false otherwise.
 */
typedef bool (*bemf_init_t)(void);

/**
 * @brief Reset internal BEMF state (without reinitializing hardware).
 *
 * Use this before restarting an open-loop ramp to clear previous detection data.
 */
typedef void (*bemf_reset_t)(void);

/**
 * @brief Process one new ADC sample (to be called every fast-loop).
 * @param floating_phase Current floating phase according to six-step pattern.
 */
typedef void (*bemf_process_t)(s_motor_phase_t floating_phase);

/**
 * @brief Get the latest BEMF status (non-blocking).
 * @param out_status Pointer to structure that receives the current status.
 */
typedef void (*bemf_get_status_t)(bemf_status_t *out_status);

/**
 * @brief Clear the zero-cross flag after it has been processed.
 */
typedef void (*bemf_clear_flag_t)(void);

/* ---------------------------------------------------------------------------
 * Service Interface Structure
 * ------------------------------------------------------------------------- */

/**
 * @brief Service interface for BEMF monitoring.
 *
 * Provides function pointers for initialization, periodic processing,
 * and retrieval of back-EMF status information.
 */
typedef struct
{
    bemf_init_t        init;        /**< Initialize the BEMF monitor */
    bemf_reset_t       reset;       /**< Reset internal states (added) ✅ */
    bemf_process_t     process;     /**< Process one fast-loop sample */
    bemf_get_status_t  get_status;  /**< Retrieve last computed status */
    bemf_clear_flag_t  clear_flag;  /**< Clear zero-cross flag */
} s_bemf_monitor_t;

/**
 * @brief Global instance of the BEMF monitoring service.
 *
 * Example usage:
 * @code
 *     SBemfMonitor->init();
 *     SBemfMonitor->reset(); // before restarting ramp
 *     SBemfMonitor->process(S_MOTOR_PHASE_A);
 *     bemf_status_t status;
 *     SBemfMonitor->get_status(&status);
 * @endcode
 */
extern s_bemf_monitor_t* SBemfMonitor;

#endif /* SERVICE_BEMF_MONITOR_H */
