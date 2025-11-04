/**
 * @file timers_callbacks.c
 * @brief Centralized HAL timer callback dispatcher.
 *
 * This module centralizes all STM32 HAL timer callbacks used in the firmware.
 * It prevents conflicts caused by multiple `HAL_TIM_PeriodElapsedCallback()`
 * definitions across different driver modules.
 *
 * Each timer-related driver implements an internal entry function (e.g.,
 * Driver_OneShot_OnTimerExpired, Driver_FastLoop_OnTimerElapsed, etc.),
 * which this dispatcher calls when the associated hardware timer triggers.
 *
 * This design provides a clean separation between:
 *   - HAL-level callbacks (in this file)
 *   - driver-level handlers (in their respective modules)
 *
 * Target MCU: STM32G473CCTx
 */

#include "i_time_oneshot.h"   // For one-shot timer ISR redirection
#include "timers_callbacks.h"
#include "bsp_utils.h"
#include <stdbool.h>

/* ========================================================================== */
/* ===  Initialization Flag  ================================================ */
/* ========================================================================== */

/** 
 * @brief Internal flag indicating whether the timer callbacks module
 *        has been initialized.
 *
 * This flag can be used by other modules to verify that the dispatcher
 * is ready before relying on its functionality.
 */
bool Timer_Callbacks_Initialized = false;

/* ========================================================================== */
/* ===  Initialization Check Function  ===================================== */
/* ========================================================================== */

/**
 * @brief Check if the timer callbacks dispatcher is initialized.
 *
 * @return true if initialized, false otherwise.
 */
bool Timer_Callbacks_IsInitialized(void)
{
    return Timer_Callbacks_Initialized;
}

/**
 * @brief Initialize the timer callbacks dispatcher.
 *
 * This function sets up any necessary state for the dispatcher.
 * It should be called once at system startup before any timers are used.
 *
 * @return true if initialization succeeded, false otherwise.
 */
bool Timer_callbacks_Init(void)
{
    if (!Timer_Callbacks_Initialized)
    {
        Timer_Callbacks_Initialized = true;
    }

    return Timer_Callbacks_Initialized;
}

/* ========================================================================== */
/* === HAL Timer Callback Dispatcher ======================================= */
/* ========================================================================== */

/**
 * @brief Common dispatcher for all HAL timer period-elapsed interrupts.
 *
 * This function is automatically called by the STM32 HAL whenever
 * any hardware timer configured with interrupts reaches its period (ARR value).
 *
 * The dispatcher checks the timer instance (`htim->Instance`) and routes
 * the event to the appropriate driver handler.
 *
 * @param htim Pointer to the HAL timer handle that generated the interrupt.
 *
 * @note This function runs in **ISR context**.
 *       Keep the operations short and avoid blocking calls or HAL delays.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* === Dispatch to One-Shot Timer Driver =============================== */
    Driver_OneShot_OnTimerExpired(htim);

    /* === Dispatch to FastLoop Driver (future extension) ================== */
    Driver_FastLoop_OnTimerElapsed(htim);

    /* === Dispatch to LowLoop Driver ==================================== */
    Driver_LowLoop_OnTimerElapsed(htim);

    /* === Additional timer modules can be added here ====================== */
    // Example:
    // Driver_PwmSync_OnTimerElapsed(htim);
    // Driver_Commutation_OnTimerElapsed(htim);
}
