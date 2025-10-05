#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include "stm32g4xx_ll_adc.h"   // LL helper to enable internal paths


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