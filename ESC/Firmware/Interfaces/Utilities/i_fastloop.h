/**
 * @file i_fastloop.h
 * @brief Abstract interface for the high-frequency control loop trigger (Fast Loop).
 *
 * This interface defines how the Control Layer is notified at a fixed rate
 * to execute the real-time control algorithm (e.g., 24 kHz FOC or six-step control).
 *
 * The implementation (Driver Layer) may use a hardware timer, PWM event,
 * or ADC conversion trigger — this interface remains hardware-agnostic.
 */

#ifndef I_FASTLOOP_H
#define I_FASTLOOP_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================================================== */
/* === Types =============================================================== */
/* ========================================================================== */

/**
 * @brief Function type for the Fast Loop callback.
 * This function is executed once per control period.
 */
typedef void (*fastloop_callback_t)(void);

/* ========================================================================== */
/* === Interface Definition =============================================== */
/* ========================================================================== */

/**
 * @brief Abstract Fast Loop interface.
 */
typedef struct
{
    /**
     * @brief Initialize the Fast Loop system.
     * Must configure all underlying hardware resources (but not start them).
     * @return true if initialization succeeds.
     */
    bool (*init)(void);

    /**
     * @brief Register a user callback to be executed at each Fast Loop cycle.
     * @param cb Pointer to the callback function (NULL disables the callback).
     */
    void (*register_callback)(fastloop_callback_t cb);

    /**
     * @brief Start periodic Fast Loop execution.
     * Enables the hardware trigger or interrupt.
     */
    void (*start)(void);

    /**
     * @brief Stop periodic Fast Loop execution.
     * Disables the hardware trigger or interrupt.
     */
    void (*stop)(void);

    /**
     * @brief Get the nominal Fast Loop frequency.
     * @return Frequency in Hz (e.g., 24000 for 24 kHz)
     */
    uint32_t (*get_frequency_hz)(void);

} i_fastloop_t;

/* ========================================================================== */
/* === Global Instance ===================================================== */
/* ========================================================================== */

/**
 * @brief Global Fast Loop interface instance.
 * Defined in /config to select the active implementation.
 */
extern i_fastloop_t* IFastLoop;

#endif /* I_FASTLOOP_H */
