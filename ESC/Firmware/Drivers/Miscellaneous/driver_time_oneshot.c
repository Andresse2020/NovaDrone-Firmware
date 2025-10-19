/**
 * @file driver_time_oneshot.c
 * @brief Hardware driver implementation of one-shot timer using a generic TIM instance.
 *
 * This driver implements the i_timer_oneshot_t interface on top of any STM32G4 TIM
 * configured in One Pulse Mode (OPM). It allows the application to schedule a single
 * callback after a specified delay in microseconds.
 *
 * Key features:
 *  - Non-blocking and deterministic
 *  - 32-bit capable (depending on the selected timer)
 *  - Hardware-timed, no CPU load between events
 *  - Callback executed in ISR context
 *
 * Default hardware: TIM5 (32-bit) on STM32G473CCTX
 */

#include "i_time_oneshot.h"
#include "tim.h"     // CubeMX-generated timer handles
#include <string.h>

/* ========================================================================== */
/* === Internal Context Structure ========================================== */
/* ========================================================================== */

/**
 * @brief Internal runtime context for one-shot scheduling.
 */
typedef struct
{
    volatile bool          is_active;      /**< True if a one-shot is currently armed */
    oneshot_callback_t     callback;       /**< Function to be called upon expiration */
    void                  *user_context;   /**< Pointer passed to the callback (user-defined) */
    uint32_t               min_delay_us;   /**< Minimum valid delay (µs) to avoid ISR latency issues */
    TIM_HandleTypeDef     *hw_timer;       /**< Pointer to the underlying HAL timer handle */
} oneshot_context_t;

/** Static context: internal state of this driver (file-scope only). */
static oneshot_context_t s_timerContext;

/* ========================================================================== */
/* === Private function declarations ======================================= */
/* ========================================================================== */
static void oneshot_hw_arm(uint32_t delay_us);
static void oneshot_hw_disarm(void);

/* ========================================================================== */
/* === Interface implementation ============================================ */
/* ========================================================================== */

/**
 * @brief Initialize the one-shot timer driver.
 *        This function binds the driver to a specific hardware timer.
 *
 * @return true if initialization succeeded, false otherwise.
 */
static bool drv_oneshot_init(void)
{
    memset((void *)&s_timerContext, 0, sizeof(s_timerContext));

    s_timerContext.min_delay_us = 5U;     // Safety minimum delay
    s_timerContext.hw_timer     = &htim5; // Default binding: TIM5 (32-bit)

    // Ensure timer is stopped and configured in One Pulse Mode
    __HAL_TIM_DISABLE(s_timerContext.hw_timer);
    // s_timerContext.hw_timer->Instance->CR1 |= TIM_CR1_OPM; // One Pulse Mode
    __HAL_TIM_ENABLE_IT(s_timerContext.hw_timer, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(s_timerContext.hw_timer, TIM_FLAG_UPDATE);

    s_timerContext.is_active = false;
    return true;
}

/**
 * @brief Start a one-shot delay with a callback.
 *
 * @param delay_us    Delay duration in microseconds.
 * @param cb          Function to call upon timer expiration.
 * @param user_ctx    User context pointer (passed to the callback).
 * @return true if the timer was successfully armed, false otherwise.
 */
static bool drv_oneshot_start(uint32_t delay_us, oneshot_callback_t cb, void *user_ctx)
{
    if (cb == NULL || s_timerContext.hw_timer == NULL)
        return false;

    if (delay_us < s_timerContext.min_delay_us)
        delay_us = s_timerContext.min_delay_us;

    // Store callback info before arming
    s_timerContext.callback      = cb;
    s_timerContext.user_context  = user_ctx;
    s_timerContext.is_active     = true;

    // Arm the hardware timer
    oneshot_hw_arm(delay_us);
    return true;
}

/**
 * @brief Cancel any currently active one-shot.
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
 * @brief Check if the one-shot timer is currently active.
 *
 * @return true if armed, false otherwise.
 */
static bool drv_oneshot_isActive(void)
{
    return s_timerContext.is_active;
}

/* ========================================================================== */
/* === Hardware control ==================================================== */
/* ========================================================================== */

/**
 * @brief Arm the hardware timer for a one-shot delay.
 *
 * @param delay_us Delay duration in microseconds.
 */
static void oneshot_hw_arm(uint32_t delay_us)
{
    TIM_HandleTypeDef *hw = s_timerContext.hw_timer;

    __HAL_TIM_DISABLE(hw);
    __HAL_TIM_CLEAR_FLAG(hw, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(hw, 0U);
    __HAL_TIM_SET_AUTORELOAD(hw, delay_us);
    __HAL_TIM_ENABLE_IT(hw, TIM_IT_UPDATE);
    __HAL_TIM_ENABLE(hw);
}

/**
 * @brief Stop and clear the hardware timer.
 */
static void oneshot_hw_disarm(void)
{
    TIM_HandleTypeDef *hw = s_timerContext.hw_timer;

    __HAL_TIM_DISABLE(hw);
    __HAL_TIM_DISABLE_IT(hw, TIM_IT_UPDATE);
    __HAL_TIM_CLEAR_FLAG(hw, TIM_FLAG_UPDATE);
}

/* ========================================================================== */
/* === Interrupt handling ================================================== */
/* ========================================================================== */

/**
 * @brief HAL callback invoked when the timer expires.
 * @note  This executes in ISR context, so keep it short and non-blocking.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (s_timerContext.hw_timer != NULL &&
        htim->Instance == s_timerContext.hw_timer->Instance)
    {
        // Snapshot user data before clearing
        oneshot_callback_t user_cb = s_timerContext.callback;
        void *user_ctx             = s_timerContext.user_context;

        // Clear internal state
        s_timerContext.is_active     = false;
        s_timerContext.callback      = NULL;
        s_timerContext.user_context  = NULL;

        __HAL_TIM_DISABLE(s_timerContext.hw_timer);

        // Execute user callback (ISR context)
        if (user_cb)
            user_cb(user_ctx);
    }
}

/* ========================================================================== */
/* === Global instance binding ============================================ */
/* ========================================================================== */

static const i_timer_oneshot_t i_timer_oneshot_driver = {
    .init     = drv_oneshot_init,
    .start    = drv_oneshot_start,
    .cancel   = drv_oneshot_cancel,
    .isActive = drv_oneshot_isActive,
};

/** Global interface instance */
i_timer_oneshot_t *IOneShotTimer = (i_timer_oneshot_t *)&i_timer_oneshot_driver;
