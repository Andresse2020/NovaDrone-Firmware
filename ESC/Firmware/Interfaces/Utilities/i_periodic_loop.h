/**
 * @file i_periodic_loop.h
 * @brief Generic interface for periodic control or service loops.
 *
 * This interface abstracts the concept of a periodic execution trigger.
 * It can represent any recurring real-time loop, such as:
 *   - High-frequency control loops (e.g., FOC, current control)
 *   - Medium-frequency loops (e.g., speed regulation)
 *   - Low-frequency loops (e.g., system monitoring, telemetry)
 *
 * The underlying implementation can rely on a timer, a PWM event,
 * an ADC end-of-conversion trigger, or any other hardware/software source.
 *
 * The interface remains fully hardware-agnostic.
 */

#ifndef I_PERIODIC_LOOP_H
#define I_PERIODIC_LOOP_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/* === Types =============================================================== */
/* ========================================================================== */

/**
 * @brief Type definition for a user-defined callback.
 * The callback is executed exactly once per loop cycle.
 */
typedef void (*periodic_callback_t)(void);

/* ========================================================================== */
/* === Interface Definition =============================================== */
/* ========================================================================== */

/**
 * @brief Abstract interface representing a periodic execution loop.
 *
 * Each implementation defines how the loop is timed and triggered.
 * For example:
 *   - A hardware timer ISR at 24 kHz
 *   - A FreeRTOS software timer at 1 kHz
 *   - A DMA completion event at variable rate
 */
typedef struct
{
    /**
     * @brief Initialize the periodic loop subsystem.
     *
     * This must prepare all required hardware or software resources
     * (timers, interrupts, etc.) but should not start execution yet.
     *
     * @return true if initialization was successful, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Register a callback executed at each loop cycle.
     *
     * Passing NULL disables the callback.
     *
     * @param cb Pointer to the function to be called periodically.
     */
    void (*register_callback)(periodic_callback_t cb);

    /**
     * @brief Start the periodic loop execution.
     *
     * Enables the timer, interrupt, or scheduling mechanism responsible
     * for triggering the registered callback.
     */
    void (*start)(void);

    /**
     * @brief Stop the periodic loop execution.
     *
     * Disables the triggering mechanism and prevents further callbacks.
     */
    void (*stop)(void);

    /**
     * @brief Retrieve the configured loop frequency.
     *
     * @return Frequency in Hz (e.g., 24000 for 24 kHz, 1000 for 1 kHz).
     */
    uint32_t (*get_frequency_hz)(void);

    /**
     * @brief (Optional) Manually trigger a single loop cycle.
     *
     * Some implementations may support forcing one callback execution,
     * useful for testing or calibration without starting continuous mode.
     */
    void (*trigger_once)(void);

} i_periodic_loop_t;


/* ========================================================================== */
/* === Global Instance ===================================================== */
/* ========================================================================== */

/**
 * @brief Global Fast Loop interface instance.
 * Defined in /config to select the active implementation.
 */
extern i_periodic_loop_t* IFastLoop;

/**
 * @brief Global Low Loop interface instance.
 * Defined in /config to select the active implementation.
 */
extern i_periodic_loop_t* ILowLoop;

#endif /* I_PERIODIC_LOOP_H */
