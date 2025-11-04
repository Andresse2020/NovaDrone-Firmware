/**
 * @file service_loop.h
 * @brief Generic High-Level Loop Service API.
 *
 * This module defines a generic service interface for managing periodic loops
 * (e.g., control loops, diagnostic loops, background tasks).
 *
 * The same API structure is used for both Fast Loop (e.g., 24 kHz)
 * and Low Loop (e.g., 1 kHz), providing:
 *   - initialization of the underlying hardware driver,
 *   - registration of user (control-layer) callback,
 *   - start/stop of the periodic trigger,
 *   - runtime profiling and diagnostics.
 *
 * The Control Layer interacts only with this API, never directly
 * with hardware drivers (IFastLoop, ILowLoop).
 */

#ifndef SERVICE_LOOP_H
#define SERVICE_LOOP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* === Type Definitions ==================================================== */
/* ========================================================================== */

/**
 * @brief Function type for a generic periodic loop callback.
 *
 * Each loop (Fast, Low, etc.) will execute one such callback once per period.
 */
typedef void (*SLoop_Callback_t)(void);

/* ========================================================================== */
/* === Generic Loop Service API =========================================== */
/* ========================================================================== */

/**
 * @brief Generic Loop Service interface.
 *
 * Represents any periodic control or monitoring loop (e.g., FastLoop, LowLoop).
 * The interface provides unified control and diagnostic functions.
 */
typedef struct
{
    /**
     * @brief Initialize the service and its underlying hardware driver.
     * @retval true  Initialization successful.
     * @retval false Initialization failed.
     */
    bool (*init)(void);

    /**
     * @brief Register a control-layer callback executed each loop tick.
     * @param cb Pointer to user callback (NULL disables callback).
     */
    void (*register_callback)(SLoop_Callback_t cb);

    /**
     * @brief Start the periodic loop execution.
     *
     * Once started, the registered callback is executed at each timer tick.
     */
    void (*start)(void);

    /**
     * @brief Stop the periodic loop execution.
     *
     * Disables the timer interrupts but preserves runtime statistics.
     */
    void (*stop)(void);

    /**
     * @brief Retrieve the nominal loop frequency (Hz).
     * @return Frequency in Hz (e.g., 24000 for FastLoop, 1000 for LowLoop).
     */
    uint32_t (*get_frequency_hz)(void);

    /**
     * @brief Retrieve runtime diagnostic statistics.
     *
     * @param tick_count   Optional pointer to total loop count.
     * @param last_exec_us Optional pointer to last execution duration (µs).
     * @param avg_exec_us  Optional pointer to average execution duration (µs).
     */
    void (*get_stats)(uint32_t* tick_count,
                      uint32_t* last_exec_us,
                      uint32_t* avg_exec_us);

} SLoop_t;

/* ========================================================================== */
/* === Global Service Instances =========================================== */
/* ========================================================================== */

/**
 * @brief Global Fast Loop Service instance (typically 24 kHz).
 *
 * Used for high-frequency control routines such as:
 *  - Current regulation (FOC)
 *  - Six-step commutation
 *  - Real-time PWM modulation
 *
 * Example:
 * @code
 *     SFastLoop->init();
 *     SFastLoop->register_callback(Motor_FastLoop);
 *     SFastLoop->start();
 * @endcode
 */
extern SLoop_t* SFastLoop;

/**
 * @brief Global Low Loop Service instance (typically 1 kHz).
 *
 * Used for lower-frequency control tasks such as:
 *  - Speed or torque regulation
 *  - System diagnostics and monitoring
 *  - Communication or telemetry management
 *
 * Example:
 * @code
 *     SLowLoop->init();
 *     SLowLoop->register_callback(Speed_Control_Loop);
 *     SLowLoop->start();
 * @endcode
 */
extern SLoop_t* SLowLoop;

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_LOOP_H */
