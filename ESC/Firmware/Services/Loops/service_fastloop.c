/**
 * @file service_fastloop.c
 * @brief Implementation of the generic Loop Service for the Fast Loop (SFastLoop).
 *
 * This module instantiates the generic Loop Service (`SLoop_t`) for the
 * high-frequency control loop (typically 24 kHz).
 *
 * Responsibilities:
 *   - Initialize the underlying Fast Loop driver (IFastLoop, TIM3-based)
 *   - Register and execute user callbacks at fixed intervals (~41.6 µs)
 *   - Measure runtime execution time for diagnostics
 *   - Maintain runtime statistics (tick count, average duration, etc.)
 *
 * The Control Layer interacts exclusively through this service and never
 * accesses hardware-level drivers directly.
 *
 * Target MCU: STM32G473CCTx
 */

#include "service_loop.h"
#include "i_periodic_loop.h"  // Provides IFastLoop driver interface
#include "i_time.h"           // Provides ITime->get_time_us()
#include <string.h>

/* ========================================================================== */
/* === Private Context ===================================================== */
/* ========================================================================== */

/**
 * @brief Runtime context structure shared by all loop services.
 *
 * Stores runtime statistics and the user callback pointer.
 * One instance is created for each loop (Fast, Low, etc.).
 */
typedef struct
{
    SLoop_Callback_t user_cb;   /**< Control-layer callback executed every tick */
    uint32_t tick_count;        /**< Total number of executed loop cycles */
    uint32_t last_exec_us;      /**< Duration of last callback (µs) */
    uint32_t avg_exec_us;       /**< Exponential moving average of duration (µs) */
    bool running;               /**< True if the loop service is currently active */
} sloop_ctx_t;

/** @brief Static instance of runtime context for the Fast Loop */
static sloop_ctx_t s_ctx = {0};

/**
 * @brief Reference to the underlying low-level driver (TIM3-based).
 *
 * Defined in driver_fastloop.c and provides access to hardware-level control.
 */
extern i_periodic_loop_t* IFastLoop;

/* ========================================================================== */
/* === Local Helper Functions ============================================== */
/* ========================================================================== */

/**
 * @brief Internal trampoline executed at every Fast Loop tick (ISR context).
 *
 * This function wraps the control-layer callback to:
 *   1. Measure its execution time in microseconds.
 *   2. Update tick count and performance statistics.
 *   3. Call the registered user callback.
 *
 * @note Keep it minimal — it runs inside the interrupt service routine.
 */
static void SFastLoop_Trampoline(void)
{
    if (s_ctx.user_cb == NULL)
        return;

    uint32_t start_us = ITime->get_time_us();

    /* Execute the user callback (e.g., Motor_FastLoop) */
    s_ctx.user_cb();

    uint32_t end_us = ITime->get_time_us();
    uint32_t delta = end_us - start_us;

    /* Update runtime statistics */
    s_ctx.tick_count++;
    s_ctx.last_exec_us = delta;
    s_ctx.avg_exec_us = (uint32_t)(0.9f * s_ctx.avg_exec_us + 0.1f * delta);
}

/* ========================================================================== */
/* === Internal Implementation ============================================ */
/* ========================================================================== */

/**
 * @brief Initialize the Fast Loop service and its driver.
 *
 * - Clears all runtime statistics and callback pointers.
 * - Initializes the underlying timer driver (TIM3).
 * - Registers the internal trampoline callback.
 *
 * @retval true  Initialization successful.
 * @retval false Driver initialization failed.
 */
static bool SFL_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));

    /* Initialize the hardware driver (TIM3-based Fast Loop) */
    if (!IFastLoop->init())
        return false;

    /* Register the ISR trampoline with the low-level driver */
    IFastLoop->register_callback(SFastLoop_Trampoline);

    return true;
}

/**
 * @brief Register the user callback executed each loop cycle.
 *
 * @param cb Pointer to user callback (NULL disables callback).
 */
static void SFL_RegisterCallback(SLoop_Callback_t cb)
{
    s_ctx.user_cb = cb;
}

/**
 * @brief Start the periodic Fast Loop execution.
 *
 * Resets runtime statistics and enables the hardware timer interrupts.
 */
static void SFL_Start(void)
{
    s_ctx.tick_count = 0;
    s_ctx.last_exec_us = 0;
    s_ctx.avg_exec_us = 0;
    s_ctx.running = true;

    IFastLoop->start();
}

/**
 * @brief Stop the periodic Fast Loop execution.
 *
 * Disables timer interrupts but preserves runtime statistics.
 */
static void SFL_Stop(void)
{
    s_ctx.running = false;
    IFastLoop->stop();
}

/**
 * @brief Retrieve the configured Fast Loop frequency in Hertz.
 *
 * @return Frequency in Hz (typically 24000 Hz).
 */
static uint32_t SFL_GetFrequencyHz(void)
{
    return IFastLoop->get_frequency_hz();
}

/**
 * @brief Retrieve runtime diagnostic statistics.
 *
 * @param tick_count   Optional pointer to total tick count.
 * @param last_exec_us Optional pointer to last execution duration (µs).
 * @param avg_exec_us  Optional pointer to average execution duration (µs).
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
/* === Global Service Instance ============================================ */
/* ========================================================================== */

/**
 * @brief Generic Loop Service instance for the Fast Loop.
 *
 * Provides the same API as any `SLoop_t` instance.
 */
static SLoop_t s_fastloop_iface = {
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
 * Exposed to the Control Layer as the unified API for 24 kHz operations.
 *
 * Example usage:
 * @code
 *     SFastLoop->init();
 *     SFastLoop->register_callback(Motor_FastLoop);
 *     SFastLoop->start();
 * @endcode
 */
SLoop_t* SFastLoop = &s_fastloop_iface;