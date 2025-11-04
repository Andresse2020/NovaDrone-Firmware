/**
 * @file driver_lowloop.c
 * @brief Hardware-level implementation of the Low Loop interface (i_periodic_loop_t) using TIM4.
 *
 * The timer instance is abstracted via a single directive to simplify maintenance
 * and portability across different STM32 families or timer assignments.
 *
 * Target MCU: STM32G473CCTx
 */

#include "i_periodic_loop.h"
#include "timers_callbacks.h"
#include "bsp_utils.h"
#include "tim.h"   // TIM handle declarations from CubeMX

/* ========================================================================== */
/* === Configuration Macro ================================================= */
/* ========================================================================== */

/**
 * @brief Define the timer used for the Low Loop.
 *
 * Change this definition if you decide to use another timer instance.
 * Example: replace &htim4 with &htim2 for another hardware mapping.
 */
#define LOWLOOP_TIMER_HANDLE     (&htim4)

/**
 * @brief Define the timer instance name for ISR binding.
 * Used to check the correct interrupt source in the HAL callback.
 */
#define LOWLOOP_TIMER_INSTANCE   TIM4

/* ========================================================================== */
/* === Private Static Variables ============================================ */
/* ========================================================================== */

/** @brief Pointer to the callback executed at each Low Loop tick. */
static periodic_callback_t s_registered_cb = NULL;

/** @brief Nominal Low Loop frequency in Hertz (e.g., 1 kHz = 1 ms period). */
static uint32_t s_lowloop_freq_hz = 1000U;

/* ========================================================================== */
/* === Local Helper Functions ============================================== */
/* ========================================================================== */

/**
 * @brief Called at each Low Loop tick (timer overflow).
 * Executes the registered callback if valid.
 */
static void Driver_LowLoop_OnTick(void)
{
    if (s_registered_cb != NULL)
    {
        s_registered_cb(); // Execute the user callback
    }
}

/* ========================================================================== */
/* === i_periodic_loop_t Implementation ==================================== */
/* ========================================================================== */

/**
 * @brief Initialize the Low Loop timer (TIM4 by default).
 */
static bool drv_lowloop_init(void)
{
    /* Ensure the timer dispatcher is initialized */
    if (!Timer_Callbacks_IsInitialized())
    {
        if (!Timer_callbacks_Init())
        {
            return false;
        }
    }

    /* Clear any existing callback */
    s_registered_cb = NULL;

    /* Stop and reset the timer before configuration */
    __HAL_TIM_DISABLE(LOWLOOP_TIMER_HANDLE);
    __HAL_TIM_SET_COUNTER(LOWLOOP_TIMER_HANDLE, 0);

    /* Basic validation: ensure timer configured with valid ARR */
    if (__HAL_TIM_GET_AUTORELOAD(LOWLOOP_TIMER_HANDLE) == 0)
        return false;

    return true;
}

/**
 * @brief Register a callback executed every Low Loop cycle.
 */
static void drv_lowloop_register_callback(periodic_callback_t cb)
{
    s_registered_cb = cb;
}

/**
 * @brief Start the periodic Low Loop execution.
 */
static void drv_lowloop_start(void)
{
    __HAL_TIM_CLEAR_FLAG(LOWLOOP_TIMER_HANDLE, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(LOWLOOP_TIMER_HANDLE);
}

/**
 * @brief Stop the periodic Low Loop execution.
 */
static void drv_lowloop_stop(void)
{
    HAL_TIM_Base_Stop_IT(LOWLOOP_TIMER_HANDLE);
}

/**
 * @brief Return the configured Low Loop frequency (Hz).
 */
static uint32_t drv_lowloop_get_frequency_hz(void)
{
    return s_lowloop_freq_hz;
}

/**
 * @brief Manually trigger one Low Loop cycle (optional).
 */
static void drv_lowloop_trigger_once(void)
{
    Driver_LowLoop_OnTick();
}

/* ========================================================================== */
/* === HAL Interrupt Binding =============================================== */
/* ========================================================================== */

/**
 * @brief HAL callback triggered on timer update event.
 */
void Driver_LowLoop_OnTimerElapsed(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == LOWLOOP_TIMER_INSTANCE)
    {
        Driver_LowLoop_OnTick();
    }
}

/* ========================================================================== */
/* === Global Interface Instance =========================================== */
/* ========================================================================== */

static i_periodic_loop_t s_driver_lowloop_iface = {
    .init              = drv_lowloop_init,
    .register_callback = drv_lowloop_register_callback,
    .start             = drv_lowloop_start,
    .stop              = drv_lowloop_stop,
    .get_frequency_hz  = drv_lowloop_get_frequency_hz,
    .trigger_once      = drv_lowloop_trigger_once,
};

/** @brief Global Low Loop interface instance (used by higher layers). */
i_periodic_loop_t* ILowLoop = &s_driver_lowloop_iface;
