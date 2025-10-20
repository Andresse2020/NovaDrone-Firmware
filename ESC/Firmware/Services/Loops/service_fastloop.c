/**
 * @file service_fastloop.c
 * @brief Implementation of the Fast Loop Service (SFastLoop).
 *
 * This module provides a service-layer wrapper for the low-level Fast Loop driver.
 * It offers a unified API to the Control Layer, handling:
 *   - initialization of the underlying driver,
 *   - callback registration and execution at a fixed control frequency,
 *   - runtime profiling (execution time measurement),
 *   - and runtime statistics (tick count, average execution time).
 *
 * The service acts as a safe intermediary between the hardware timer interrupt
 * and the motor control logic. The Control Layer should always use this API
 * instead of directly accessing the driver-level IFastLoop interface.
 */

#include "service_loop.h"
#include "i_fastloop.h" // Provides IFastLoop low-level driver interface
#include "i_time.h"   // Provides ITime->get_time_us() for microsecond timing
#include <string.h>

/* ========================================================================== */
/* === Private Context ===================================================== */
/* ========================================================================== */

/**
 * @brief Internal context structure storing all runtime data related to
 *        the Fast Loop service.
 *
 * This includes the user callback (from the Control Layer) and various
 * runtime metrics for performance monitoring.
 */
typedef struct
{
    SFastLoop_Callback_t user_cb;   /**< Control-layer callback executed at each tick */
    uint32_t tick_count;            /**< Total number of executed Fast Loop cycles */
    uint32_t last_exec_us;          /**< Execution duration of last callback (µs) */
    uint32_t avg_exec_us;           /**< Exponential moving average of execution time (µs) */
    bool running;                   /**< True if the Fast Loop service is currently active */
} sfastloop_ctx_t;

/** Global static context instance */
static sfastloop_ctx_t s_ctx = {0};

/**
 * @brief Reference to the underlying low-level driver (TIM3-based Fast Loop).
 *
 * This pointer is defined in the driver layer (driver_fastloop.c)
 * and exposes hardware-level control of the periodic timer interrupt.
 */
extern i_fastloop_t* IFastLoop;

/* ========================================================================== */
/* === Local Helper ======================================================== */
/* ========================================================================== */

/**
 * @brief Internal trampoline function executed at each Fast Loop interrupt.
 *
 * This function is called by the driver through IFastLoop->register_callback().
 * It performs the following actions:
 *   1. Measures the execution time of the user callback in microseconds.
 *   2. Updates runtime statistics (tick counter, last and average duration).
 *   3. Calls the registered control-layer callback safely.
 *
 * Note: This function runs in an ISR context — avoid blocking operations.
 */
static void SFastLoop_Trampoline(void)
{
    /* Skip if no user callback is registered */
    if (s_ctx.user_cb == NULL)
        return;

    /* Record start timestamp in microseconds */
    uint32_t start_us = ITime->get_time_us();

    /* Execute control-layer callback (e.g., Motor_FastLoop) */
    s_ctx.user_cb();

    /* Measure elapsed time and update statistics */
    uint32_t end_us = ITime->get_time_us();
    uint32_t delta = end_us - start_us;  /* wrap-safe with 32-bit arithmetic */

    s_ctx.tick_count++;                  /* Increment total loop counter */
    s_ctx.last_exec_us = delta;          /* Store duration of last execution */

    /* Compute exponential moving average (EMA) for smoother readings */
    s_ctx.avg_exec_us = (uint32_t)(0.9f * s_ctx.avg_exec_us + 0.1f * delta);
}

/* ========================================================================== */
/* === Internal Implementation ============================================ */
/* ========================================================================== */

/**
 * @brief Initialize the Fast Loop service and underlying driver.
 *
 * - Clears all runtime statistics and callback pointers.
 * - Initializes the low-level hardware driver (IFastLoop->init()).
 * - Registers the internal trampoline as the driver-level callback.
 *
 * @retval true  Initialization successful.
 * @retval false Initialization failed (e.g., driver error).
 */
static bool SFL_Init(void)
{
    /* Reset context to known state */
    memset(&s_ctx, 0, sizeof(s_ctx));

    /* Initialize low-level driver (e.g., TIM3) */
    if (!IFastLoop->init())
        return false;

    /* Register internal trampoline to receive periodic events from driver */
    IFastLoop->register_callback(SFastLoop_Trampoline);
    return true;
}

/**
 * @brief Register the user-defined callback executed at each Fast Loop cycle.
 *
 * @param cb Pointer to user callback function.
 *           Pass NULL to disable callback execution.
 */
static void SFL_RegisterCallback(SFastLoop_Callback_t cb)
{
    s_ctx.user_cb = cb;
}

/**
 * @brief Start the Fast Loop execution.
 *
 * - Resets runtime statistics.
 * - Enables periodic interrupts from the driver (IFastLoop->start()).
 */
static void SFL_Start(void)
{
    /* Reset statistics before starting */
    s_ctx.tick_count = 0;
    s_ctx.last_exec_us = 0;
    s_ctx.avg_exec_us = 0;
    s_ctx.running = true;

    /* Start low-level periodic trigger */
    IFastLoop->start();
}

/**
 * @brief Stop the Fast Loop execution.
 *
 * - Disables the periodic interrupt (IFastLoop->stop()).
 * - Keeps statistics for inspection until next start.
 */
static void SFL_Stop(void)
{
    s_ctx.running = false;
    IFastLoop->stop();
}

/**
 * @brief Get the configured nominal Fast Loop frequency in Hz.
 *
 * @return Frequency in Hertz (e.g., 24000 for 24 kHz).
 */
static uint32_t SFL_GetFrequencyHz(void)
{
    return IFastLoop->get_frequency_hz();
}

/**
 * @brief Retrieve runtime diagnostic statistics from the Fast Loop context.
 *
 * @param tick_count   Optional pointer to store total tick count.
 * @param last_exec_us Optional pointer to store last execution duration (µs).
 * @param avg_exec_us  Optional pointer to store averaged execution duration (µs).
 *
 * Pass NULL for any unused parameter.
 */
static void SFL_GetStats(uint32_t* tick_count,
                         uint32_t* last_exec_us,
                         uint32_t* avg_exec_us)
{
    if (tick_count)   *tick_count   = s_ctx.tick_count;
    if (last_exec_us) *last_exec_us = s_ctx.last_exec_us;
    if (avg_exec_us)  *avg_exec_us  = s_ctx.avg_exec_us;
}

/* ========================================================================== */
/* === Global Instance ===================================================== */
/* ========================================================================== */

/**
 * @brief Static structure implementing the Fast Loop service API.
 *
 * The structure members map to all SFastLoop_* function implementations.
 * This object is exposed globally through the pointer `SFastLoop`.
 */
static SFastLoop_t s_fastloop_iface = {
    .init              = SFL_Init,
    .register_callback = SFL_RegisterCallback,
    .start             = SFL_Start,
    .stop              = SFL_Stop,
    .get_frequency_hz  = SFL_GetFrequencyHz,
    .get_stats         = SFL_GetStats,
};

/**
 * @brief Global pointer to the Fast Loop Service instance.
 *
 * This is the main entry point for the Control Layer.
 * Example usage:
 * @code
 *     SFastLoop->init();
 *     SFastLoop->register_callback(Motor_FastLoop);
 *     SFastLoop->start();
 * @endcode
 */
SFastLoop_t* SFastLoop = &s_fastloop_iface;
