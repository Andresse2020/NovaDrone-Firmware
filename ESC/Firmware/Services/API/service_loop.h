/**
 * @file service_fastloop.h
 * @brief High-level Loop Service API.
 *
 * This service encapsulates the complete Loop management:
 *  - initialization of the underlying hardware driver,
 *  - registration of the control-layer callback,
 *  - start/stop of the periodic trigger,
 *  - runtime profiling and diagnostics.
 *
 * The Control Layer interacts exclusively with this object.
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
 * @brief Function type for the Fast Loop callback.
 *
 * The Control Layer must provide a callback of this type to be executed
 * once per control period (e.g., 24 kHz).
 */
typedef void (*SFastLoop_Callback_t)(void);

/* ========================================================================== */
/* ===        Loop Service Object =========================================== */
/* ========================================================================== */

/**
 * @brief Structure defining the Fast Loop Service API.
 *
 * This object provides all required methods to control the periodic fast loop.
 * Each method must be accessed via the global instance pointer `SFastLoop`.
 */
typedef struct
{
    /**
     * @brief Initialize the service and underlying hardware driver.
     * @retval true  Initialization successful
     * @retval false Initialization failed
     */
    bool (*init)(void);

    /**
     * @brief Register the Control Layer callback executed each Fast Loop tick.
     * @param cb Pointer to user callback (NULL disables)
     */
    void (*register_callback)(SFastLoop_Callback_t cb);

    /**
     * @brief Start the periodic Fast Loop execution.
     */
    void (*start)(void);

    /**
     * @brief Stop the periodic Fast Loop execution.
     */
    void (*stop)(void);

    /**
     * @brief Get the nominal Fast Loop frequency (Hz).
     * @return Frequency in Hz
     */
    uint32_t (*get_frequency_hz)(void);

    /**
     * @brief Retrieve runtime statistics (diagnostic use).
     * @param tick_count   Optional pointer to total loop count
     * @param last_exec_us Optional pointer to last execution time (µs)
     * @param avg_exec_us  Optional pointer to average execution time (µs)
     */
    void (*get_stats)(uint32_t* tick_count,
                      uint32_t* last_exec_us,
                      uint32_t* avg_exec_us);

} SFastLoop_t;

/**
 * @brief Global Fast Loop Service instance.
 *
 * Example usage:
 * @code
 *     SFastLoop->init();
 *     SFastLoop->register_callback(Motor_FastLoop);
 *     SFastLoop->start();
 * @endcode
 */
extern SFastLoop_t* SFastLoop;

#ifdef __cplusplus
}
#endif

#endif /* SERVICE_LOOP_H */
