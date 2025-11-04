/**
 * @file service_lowloop.c
 * @brief Implementation of the generic Loop Service for the Low Loop (SLowLoop).
 *
 * This module instantiates the generic Loop Service (`SLoop_t`) for the
 * low-frequency control loop (typically 1 kHz).
 *
 * Responsibilities:
 *   - Initialize the underlying low-level driver (ILowLoop, TIM4-based)
 *   - Register and execute user callbacks at fixed intervals (1 ms)
 *   - Measure runtime execution time for diagnostics
 *   - Maintain runtime statistics (tick count, average duration, etc.)
 *
 * The Control Layer interacts only through this service, never directly with
 * hardware-level interfaces (ILowLoop).
 *
 * Target MCU: STM32G473CCTx
 */

#include "service_loop.h"
#include "i_periodic_loop.h"  // Provides ILowLoop driver interface
#include "i_time.h"           // Provides ITime->get_time_us()
#include <string.h>

/* ========================================================================== */
/* === Private Context ===================================================== */
/* ========================================================================== */

/**
 * @brief Runtime context structure shared by all generic loop services.
 *
 * Stores runtime statistics and the registered user callback.
 * One instance exists per service (Fast, Low, etc.).
 */
typedef struct
{
    SLoop_Callback_t user_cb;   /**< Control-layer callback executed every tick */
    uint32_t tick_count;        /**< Total number of executed loop cycles */
    uint32_t last_exec_us;      /**< Duration of last callback (µs) */
    uint32_t avg_exec_us;       /**< Exponential moving average of duration (µs) */
    bool running;               /**< True if the loop service is currently active */
} sloop_ctx_t;

/** @brief Static instance of runtime context for the Low Loop */
static sloop_ctx_t s_ctx = {0};

/**
 * @brief Reference to the low-level driver implementation (TIM4-based).
 *
 * Defined in driver_lowloop.c and provides direct access to the hardware timer.
 */
extern i_periodic_loop_t* ILowLoop;

/* ========================================================================== */
/* === Local Helper Functions ============================================== */
/* ========================================================================== */

/**
 * @brief Trampoline executed at every timer interrupt (ISR context).
 *
 * This internal function acts as a safe wrapper around the user callback:
 *   1. Measures callback execution duration.
 *   2. Updates tick count and performance statistics.
 *   3. Invokes the registered control-layer callback.
 *
 * @note Keep this function minimal — it runs inside an interrupt.
 */
static void SLowLoop_Trampoline(void)
{
    if (s_ctx.user_cb == NULL)
        return;

    uint32_t start_us = ITime->get_time_us();

    /* Execute user-defined control callback */
    s_ctx.user_cb();

    uint32_t end_us = ITime->get_time_us();
    uint32_t delta = end_us - start_us;

    /* Update performance metrics */
    s_ctx.tick_count++;
    s_ctx.last_exec_us = delta;
    s_ctx.avg_exec_us = (uint32_t)(0.9f * s_ctx.avg_exec_us + 0.1f * delta);
}

/* ========================================================================== */
/* === Internal Implementation ============================================ */
/* ========================================================================== */

/**
 * @brief Initialize the Low Loop service and the associated driver.
 *
 * - Resets all runtime statistics.
 * - Initializes the TIM4 hardware driver.
 * - Registers the internal trampoline callback.
 *
 * @retval true  Initialization successful.
 * @retval false Driver initialization failed.
 */
static bool SLL_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));

    /* Initialize low-level driver (TIM4) */
    if (!ILowLoop->init())
        return false;

    /* Register internal ISR callback */
    ILowLoop->register_callback(SLowLoop_Trampoline);

    return true;
}

/**
 * @brief Register the user callback executed at each loop cycle.
 *
 * @param cb Pointer to user function (NULL disables callback).
 */
static void SLL_RegisterCallback(SLoop_Callback_t cb)
{
    s_ctx.user_cb = cb;
}

/**
 * @brief Start periodic Low Loop execution.
 *
 * Resets all runtime counters and enables timer interrupts.
 */
static void SLL_Start(void)
{
    s_ctx.tick_count = 0;
    s_ctx.last_exec_us = 0;
    s_ctx.avg_exec_us = 0;
    s_ctx.running = true;

    ILowLoop->start();
}

/**
 * @brief Stop periodic Low Loop execution.
 *
 * Disables timer interrupts and preserves statistics for diagnostics.
 */
static void SLL_Stop(void)
{
    s_ctx.running = false;
    ILowLoop->stop();
}

/**
 * @brief Retrieve the configured Low Loop frequency in Hertz.
 *
 * @return Frequency in Hz (typically 1000 Hz).
 */
static uint32_t SLL_GetFrequencyHz(void)
{
    return ILowLoop->get_frequency_hz();
}

/**
 * @brief Retrieve runtime performance statistics.
 *
 * @param tick_count   Optional pointer to receive total tick count.
 * @param last_exec_us Optional pointer to last callback duration (µs).
 * @param avg_exec_us  Optional pointer to average duration (µs).
 */
static void SLL_GetStats(uint32_t* tick_count,
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
 * @brief Generic Loop Service instance for the Low Loop.
 *
 * All function pointers are mapped to the internal implementations defined above.
 */
static SLoop_t s_lowloop_iface = {
    .init              = SLL_Init,
    .register_callback = SLL_RegisterCallback,
    .start             = SLL_Start,
    .stop              = SLL_Stop,
    .get_frequency_hz  = SLL_GetFrequencyHz,
    .get_stats         = SLL_GetStats,
};

/**
 * @brief Global pointer to the Low Loop Service instance.
 *
 * Exposed to the Control Layer as the unified API for 1 kHz tasks.
 *
 * Example usage:
 * @code
 *     SLowLoop->init();
 *     SLowLoop->register_callback(Speed_Control_Loop);
 *     SLowLoop->start();
 * @endcode
 */
SLoop_t* SLowLoop = &s_lowloop_iface;