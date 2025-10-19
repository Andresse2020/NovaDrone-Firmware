/**
 * @file i_time_oneshot.h
 * @brief Abstract interface for one-shot hardware timers.
 *
 * This interface defines a hardware-agnostic API for scheduling
 * single-shot (one-time) callbacks after a delay in microseconds.
 *
 * The implementation is non-blocking: once scheduled, the timer
 * counts autonomously in hardware and triggers an interrupt
 * upon expiration, invoking the user callback exactly once.
 *
 * Typical implementation: TIM5 (32-bit) in One Pulse Mode.
 */

#ifndef I_TIMER_ONESHOT_H
#define I_TIMER_ONESHOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* === Callback Type ==================================================== */

/**
 * @brief Callback type for one-shot timer expiration.
 * @param context User-defined context pointer (can be NULL).
 *
 * @note Executed in ISR context: must be short and non-blocking.
 */
typedef void (*oneshot_callback_t)(void *context);

/* === Interface ======================================================== */

typedef struct
{
    /**
     * @brief Initialize the one-shot timer subsystem.
     * This function configures the underlying hardware (e.g., TIM5)
     * and enables its interrupt.
     * @return true if initialization succeeded, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Schedule a one-shot callback after a delay in microseconds.
     * If another timer was active, it is replaced by the new one.
     * @param delay_us Delay duration in microseconds.
     * @param cb Callback to invoke when timer expires (ISR context).
     * @param ctx Pointer passed to the callback (user context).
     * @return true if scheduled successfully, false if invalid or busy.
     */
    bool (*start)(uint32_t delay_us, oneshot_callback_t cb, void *ctx);

    /**
     * @brief Cancel the currently active one-shot (if any).
     * Does nothing if no timer is active.
     */
    void (*cancel)(void);

    /**
     * @brief Check if a one-shot is currently armed.
     * @return true if armed, false if idle.
     */
    bool (*isActive)(void);

} i_timer_oneshot_t;

/* === Global instance ================================================== */

/**
 * @brief Global instance of the one-shot timer interface.
 * Defined in configuration (platform-specific binding).
 */
extern i_timer_oneshot_t* IOneShotTimer;

#ifdef __cplusplus
}
#endif

#endif /* I_TIMER_ONESHOT_H */
