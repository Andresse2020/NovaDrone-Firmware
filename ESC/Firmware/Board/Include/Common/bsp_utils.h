#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_adc.h"   // LL helper to enable internal paths


/* Private defines -----------------------------------------------------------*/
#define PWM_A_L_Pin GPIO_PIN_13
#define PWM_A_L_GPIO_Port GPIOC
#define Current_Sens_1_Pin GPIO_PIN_0
#define Current_Sens_1_GPIO_Port GPIOA
#define Current_Sens_2_Pin GPIO_PIN_6
#define Current_Sens_2_GPIO_Port GPIOA
#define V12_Measure_Pin GPIO_PIN_13
#define V12_Measure_GPIO_Port GPIOB
#define V3_3_Measure_Pin GPIO_PIN_14
#define V3_3_Measure_GPIO_Port GPIOB
#define Vbus_Measure_Pin GPIO_PIN_15
#define Vbus_Measure_GPIO_Port GPIOB
#define PWM_A_H_Pin GPIO_PIN_8
#define PWM_A_H_GPIO_Port GPIOA
#define PWM_B_H_Pin GPIO_PIN_9
#define PWM_B_H_GPIO_Port GPIOA
#define PWM_C_H_Pin GPIO_PIN_10
#define PWM_C_H_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_11
#define LED_GPIO_Port GPIOA
#define PWM_B_L_Pin GPIO_PIN_12
#define PWM_B_L_GPIO_Port GPIOA
#define CAN_STB_Pin GPIO_PIN_4
#define CAN_STB_GPIO_Port GPIOB
#define CAN_RX_Pin GPIO_PIN_5
#define CAN_RX_GPIO_Port GPIOB
#define CAN_TX_Pin GPIO_PIN_6
#define CAN_TX_GPIO_Port GPIOB
#define PWM_C_L_Pin GPIO_PIN_9
#define PWM_C_L_GPIO_Port GPIOB

// Common utility functions and definitions

void Error_Handler(void); // Error handler function

/**
 * @brief Debug print function via UART with variable arguments
 * @param huart Pointer to UART handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 * @retval HAL_StatusTypeDef Status of transmission
 */
HAL_StatusTypeDef Debug_Printf(UART_HandleTypeDef *huart, const char *format, ...);


#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */