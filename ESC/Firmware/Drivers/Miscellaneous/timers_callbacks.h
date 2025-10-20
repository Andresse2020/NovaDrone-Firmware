/**
 * @file timers_callbacks.h
 * @brief Centralized dispatcher for HAL timer callbacks.
 *
 * This module groups all STM32 HAL timer callbacks into a single
 * implementation point, preventing callback duplication across drivers.
 *
 * Each timer-related driver (e.g., one-shot, fast-loop, PWM sync)
 * can expose its own internal handler which is then called from this
 * common dispatcher.
 *
 * By isolating HAL callbacks here, the rest of the system (drivers, services)
 * remains decoupled from HAL-level functions, improving modularity and clarity.
 *
 * Target MCU: STM32G473CCTx
 */

#ifndef TIMERS_CALLBACKS_H
#define TIMERS_CALLBACKS_H

#include "tim.h"  // Provides TIM_HandleTypeDef

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if the timer callbacks dispatcher is initialized.
 *
 * @return true if initialized, false otherwise.
 */
bool Timer_Callbacks_IsInitialized(void);

/**
 * @brief Initialize the timer callbacks dispatcher.
 *
 * This function sets up any necessary state for the dispatcher.
 * It should be called once at system startup before any timers are used.
 *
 * @return true if initialization succeeded, false otherwise.
 */
bool Timer_callbacks_Init(void);

/**
 * @brief ISR entry point for the one-shot timer driver (TIM5).
 *
 * This function is implemented in `driver_time_oneshot.c` and is responsible
 * for handling the expiration of a one-shot delay.
 *
 * @param htim Pointer to the HAL timer handle that triggered the interrupt.
 */
void Driver_OneShot_OnTimerExpired(TIM_HandleTypeDef *htim);

/**
 * @brief ISR entry point for the Fast Loop driver (e.g., TIM3).
 *
 * This function is implemented in `driver_fastloop.c` and is responsible
 * for handling the periodic Fast Loop tick.
 *
 * @param htim Pointer to the HAL timer handle that triggered the interrupt.
 */
void Driver_FastLoop_OnTimerElapsed(TIM_HandleTypeDef *htim);



#ifdef __cplusplus
}
#endif

#endif /* TIMERS_CALLBACKS_H */
