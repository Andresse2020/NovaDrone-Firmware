#include "i_system.h"
#include "bsp_utils.h"
#include "clock_config.h"
#include "gpio.h"
#include "usart.h"
#include "fdcan.h"
#include "adc.h"
#include "tim.h"
#include "dma.h"

/**
 * @brief Initialize system core (HAL, clock)
 *
 * This function should be called once at system startup.
 * It initializes HAL and system clock.
 *
 * @return true if all initializations succeed, false otherwise
 */
i_status_t DSystem_Init(void)
{
    // Initialize HAL Library
    if (HAL_Init() != HAL_OK)
    {
        return I_ERROR; // HAL init failed
    }

    // Configure the system clock
    SystemClock_Config();

    return I_OK; // System core initializations succeeded
}

/**
 * @brief Initialize essential drivers and peripherals.
 *
 * This function initializes all low-level hardware drivers required 
 * for the system to operate. It covers GPIOs, communication interfaces 
 * (UART, FDCAN), ADCs, timers, and other essential peripherals.
 *
 * @return I_OK if all initializations succeed, I_ERROR otherwise.
 */
i_status_t Driver_Init(void)
{
    // Initialize all GPIO pins configured via CubeMX.
    // Sets pin modes, speeds, pull-ups/downs, and initial output levels.
    MX_GPIO_Init();

    // Enable DMA controller clock
    MX_DMA_Init();

    // Initialize UART2 for communication (e.g., debug or data transfer).
    MX_USART2_UART_Init();

    // Initialize FDCAN2 interface for CAN communication.
    MX_FDCAN2_Init();

    // Initialize ADC1, ADC2 and ADC3 for analog input readings (e.g., sensors).
    MX_ADC1_Init();
    MX_ADC2_Init();
    MX_ADC3_Init();
    MX_ADC4_Init();
    MX_ADC5_Init();

    // Initialize TIM1, TIM2, TIM5, and TIM6 timers for various timing functions.
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM5_Init();
    MX_TIM6_Init();

    // TODO: Add other essential peripheral initializations here if needed
    // For example: SPI, I2C, additional timers, DAC, etc.

    // Return success status if all initializations completed without errors.
    return I_OK;
}


/**
 * @brief Perform a software reset of the MCU.
 *
 * This function triggers a complete reset of the microcontroller
 * using the HAL library. It is equivalent to pressing the
 * hardware reset button on the board.
 *
 * Notes:
 * - All peripheral registers and volatile variables will be reset.
 * - Execution will not continue beyond this point.
 * - Useful for recovering from critical errors or restarting the system.
 */
void DSystem_Reset(void)
{
    // Call HAL function to trigger a system reset
    HAL_NVIC_SystemReset();
}