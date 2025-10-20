/**
 * @file driver_time_oneshot.c
 * @brief Hardware driver implementation of one-shot timer using a generic TIM instance.
 *
 * This driver implements a hardware-based one-shot timer on top of any STM32G4 TIM peripheral
 * configured in **One Pulse Mode (OPM)**. It provides deterministic timing without CPU load
 * during waiting periods.
 *
 * When started, the timer counts for a specified delay (in microseconds) and triggers
 * an interrupt exactly once upon expiration. The assigned callback is executed
 * immediately inside the ISR context.
 *
 * This driver implements the `i_timer_oneshot_t` interface and can be used by
 * higher layers (e.g., Service Layer) to schedule delayed or time-triggered tasks.
 *
 * **Target hardware:** TIM5 (32-bit timer on STM32G473CCTx)
 */

#include "i_time_oneshot.h"
#include "timers_callbacks.h"
#include "tim.h"     // CubeMX-generated HAL handles (htimX)
#include <string.h>

/* ========================================================================== */
/* === Internal Context Structure ========================================== */
/* ========================================================================== */

/**
 * @brief Internal runtime context of the One-Shot Timer driver.
 *
 * This structure maintains the current state of the driver, including:
 *  - whether a one-shot is active,
 *  - the registered callback and user context,
 *  - the selected hardware timer instance,
 *  - and the minimum allowable delay to prevent ISR latency issues.
 *
 * The context is static (single instance per driver).
 */
typedef struct
{
    volatile bool          is_active;      /**< True if a one-shot is currently armed */
    oneshot_callback_t     callback;       /**< Function pointer executed at timer expiration */
    void                  *user_context;   /**< User-defined context pointer passed to callback */
    uint32_t               min_delay_us;   /**< Minimum delay allowed to prevent 0-length pulse */
    TIM_HandleTypeDef     *hw_timer;       /**< Pointer to the underlying HAL timer (e.g. &htim5) */
} oneshot_context_t;

/** Static context instance for this driver (file-scope only). */
static oneshot_context_t s_timerContext;

/* ========================================================================== */
/* === Private Prototypes ================================================== */
/* ========================================================================== */

/**
 * @brief Arm the hardware timer for a one-shot delay.
 * @param delay_us Desired delay duration in microseconds.
 */
static void oneshot_hw_arm(uint32_t delay_us);

/**
 * @brief Disarm (stop) the hardware timer and clear interrupt flags.
 */
static void oneshot_hw_disarm(void);

/* ========================================================================== */
/* === Interface Implementation =========================================== */
/* ========================================================================== */

/**
 * @brief Initialize the one-shot timer driver.
 *
 * This function binds the driver to a specific hardware timer (TIM5 by default),
 * clears any previous configuration, and ensures the timer interrupt is enabled.
 *
 * @retval true  Initialization succeeded.
 * @retval false Initialization failed.
 */
static bool drv_oneshot_init(void)
{
    /* Ensure timer callbacks dispatcher is initialized */
    if (!Timer_Callbacks_IsInitialized())
    {
        if (!Timer_callbacks_Init())
        {
            return false;
        }
    }
    
    /* Reset internal state */
    memset(&s_timerContext, 0, sizeof(s_timerContext));

    /* Configure driver defaults */
    s_timerContext.min_delay_us = 5U;     // Minimum delay (5 µs safety margin)
    s_timerContext.hw_timer     = &htim5; // Default to TIM5 (32-bit timer)

    /* Ensure timer is stopped and ready */
    __HAL_TIM_DISABLE(s_timerContext.hw_timer);
    __HAL_TIM_ENABLE_IT(s_timerContext.hw_timer, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(s_timerContext.hw_timer, TIM_FLAG_UPDATE);

    s_timerContext.is_active = false;
    return true;
}

/**
 * @brief Start a one-shot delay and register a callback.
 *
 * When called, the timer is configured to trigger exactly once after `delay_us`
 * microseconds. Upon expiration, the registered callback is executed in ISR context.
 *
 * @param delay_us  Delay duration in microseconds.
 * @param cb        Function pointer to call when timer expires.
 * @param user_ctx  Optional user context pointer passed to callback.
 *
 * @retval true  Timer armed successfully.
 * @retval false Invalid parameters or hardware unavailable.
 */
static bool drv_oneshot_start(uint32_t delay_us, oneshot_callback_t cb, void *user_ctx)
{
    /* Validate input parameters */
    if (cb == NULL || s_timerContext.hw_timer == NULL)
        return false;

    /* Enforce minimum safe delay */
    if (delay_us < s_timerContext.min_delay_us)
        delay_us = s_timerContext.min_delay_us;

    /* Store callback and context before arming hardware */
    s_timerContext.callback      = cb;
    s_timerContext.user_context  = user_ctx;
    s_timerContext.is_active     = true;

    /* Start hardware timer in one-shot mode */
    oneshot_hw_arm(delay_us);
    return true;
}

/**
 * @brief Cancel any currently active one-shot operation.
 *
 * If a timer is currently running, this function immediately stops it and
 * clears all internal state. The callback will not be executed.
 */
static void drv_oneshot_cancel(void)
{
    if (s_timerContext.hw_timer == NULL)
        return;

    oneshot_hw_disarm();

    s_timerContext.is_active     = false;
    s_timerContext.callback      = NULL;
    s_timerContext.user_context  = NULL;
}

/**
 * @brief Check if a one-shot timer is currently active.
 *
 * @retval true  A timer is currently running.
 * @retval false No active timer.
 */
static bool drv_oneshot_isActive(void)
{
    return s_timerContext.is_active;
}

/* ========================================================================== */
/* === Hardware Functions ================================================== */
/* ========================================================================== */

/**
 * @brief Arm the underlying hardware timer to generate a one-shot delay.
 *
 * This function:
 *   - Resets the timer counter to 0.
 *   - Sets the auto-reload value to match the requested delay.
 *   - Enables the update interrupt.
 *   - Starts the timer.
 *
 * The timer will automatically generate an interrupt when it reaches ARR.
 *
 * @param delay_us Delay duration in microseconds.
 */
static void oneshot_hw_arm(uint32_t delay_us)
{
    TIM_HandleTypeDef *hw = s_timerContext.hw_timer;

    /* Ensure clean start */
    __HAL_TIM_DISABLE(hw);
    __HAL_TIM_CLEAR_FLAG(hw, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(hw, 0U);

    /* Configure ARR (auto-reload) as delay duration */
    __HAL_TIM_SET_AUTORELOAD(hw, delay_us);

    /* Enable interrupt and start counting */
    __HAL_TIM_ENABLE_IT(hw, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(hw);
}

/**
 * @brief Stop and clear the hardware timer.
 *
 * Disables the timer and its interrupt, and clears any pending flags.
 * Used when canceling an active one-shot or after expiration.
 */
static void oneshot_hw_disarm(void)
{
    TIM_HandleTypeDef *hw = s_timerContext.hw_timer;

    __HAL_TIM_DISABLE(hw);
    __HAL_TIM_DISABLE_IT(hw, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(hw, TIM_FLAG_UPDATE);
}

/* ========================================================================== */
/* === ISR Entry Point (Called by Dispatcher) ============================== */
/* ========================================================================== */

/**
 * @brief ISR entry point called by the global timer dispatcher
 *        when TIM5 triggers an update event (one-shot expiration).
 *
 * This function:
 *   - Verifies that the event belongs to this driver (timer match).
 *   - Clears internal state.
 *   - Executes the registered user callback in ISR context.
 *
 * @param htim Pointer to the HAL timer handle that generated the interrupt.
 *
 * @note  This function executes in **ISR context**.
 *        The callback must be short and non-blocking.
 */
void Driver_OneShot_OnTimerExpired(TIM_HandleTypeDef *htim)
{
    /* Ignore unrelated timers */
    if (s_timerContext.hw_timer == NULL ||
        htim->Instance != s_timerContext.hw_timer->Instance)
    {
        return;
    }

    /* Snapshot user callback before clearing internal state */
    oneshot_callback_t user_cb = s_timerContext.callback;
    void *user_ctx             = s_timerContext.user_context;

    /* Clear driver runtime state */
    s_timerContext.is_active     = false;
    s_timerContext.callback      = NULL;
    s_timerContext.user_context  = NULL;

    /* Stop timer hardware */
    __HAL_TIM_DISABLE(s_timerContext.hw_timer);

    /* Execute user callback (ISR context) */
    if (user_cb)
        user_cb(user_ctx);
}

/* ========================================================================== */
/* === Global Interface Binding =========================================== */
/* ========================================================================== */

/**
 * @brief Static interface instance mapping driver functions to the
 *        i_timer_oneshot_t abstraction.
 *
 * Higher layers use the `IOneShotTimer` pointer to access these methods.
 */
static const i_timer_oneshot_t i_timer_oneshot_driver = {
    .init     = drv_oneshot_init,
    .start    = drv_oneshot_start,
    .cancel   = drv_oneshot_cancel,
    .isActive = drv_oneshot_isActive,
};

/**
 * @brief Global driver interface instance.
 *
 * Example usage:
 * @code
 *     IOneShotTimer->init();
 *     IOneShotTimer->start(500000, MyCallback, NULL);
 * @endcode
 */
i_timer_oneshot_t *IOneShotTimer = (i_timer_oneshot_t *)&i_timer_oneshot_driver;
