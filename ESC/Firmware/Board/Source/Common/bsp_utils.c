#include <stdio.h>   // For vsnprintf
#include <stdarg.h>  // For va_list, va_start, va_end
#include "bsp_utils.h"
#include "gpio.h"

// LED configuration (adapt to your hardware)
#define ERROR_LED_PORT LED_GPIO_Port
#define ERROR_LED_PIN  LED_Pin

/**
 * @brief Error Handler - LED blink pattern for error signaling
 * @details Pattern: 10 rapid blinks at 50Hz, then 1s solid ON, repeat indefinitely
 * @note This function never returns
 */
void Error_Handler(void)
{
    // Disable interrupts
    // __disable_irq();
    
    // Infinite loop with distinctive pattern
    while (1)
    {
        // 10 rapid blinks at 50Hz
        for (int i = 0; i < 10; i++)
        {
            HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_SET);
            HAL_Delay(50);
            HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_RESET);
            HAL_Delay(50);
        }
        
        // Solid ON for 1 second
        HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_SET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(ERROR_LED_PORT, ERROR_LED_PIN, GPIO_PIN_RESET);
    }
}


/**
 * @brief Debug print function via UART with variable arguments
 * @param huart Pointer to UART handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 * @retval HAL_StatusTypeDef Status of transmission
 * @note Automatically appends \r\n at the end
 */
HAL_StatusTypeDef Debug_Printf(UART_HandleTypeDef *huart, const char *format, ...)
{
    char buffer[256];
    va_list args;
    
    // Format the string with variable arguments
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer) - 2, format, args);  // Reserve 2 chars for \r\n
    va_end(args);
    
    // Check for formatting errors or buffer overflow
    if (len < 0 || (size_t)len >= sizeof(buffer) - 2) {
        return HAL_ERROR;
    }
    
    // Append line ending
    buffer[len] = '\r';
    buffer[len + 1] = '\n';
    len += 2;
    
    // Transmit via UART
    return HAL_UART_Transmit(huart, (uint8_t*)buffer, (uint16_t)len, HAL_MAX_DELAY);
}