#include "i_time.h"
#include "bsp_utils.h"
#include "tim.h"

/* -------------------------------------------------------------------------- */
/*                   Local static helper function definitions                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize DWT for cycle counting
 * @note Must be called once at startup
 */
void DWT_Init(void)
{
    // Enable TRC (Trace)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // Reset cycle counter
    DWT->CYCCNT = 0;
    
    // Enable cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/**
 * @brief Initialize the hardware timer for microsecond delays.
 * 
 * This function starts TIM7 in base mode to be used for
 * generating microsecond delays.
 * 
 * @return true if initialization is successful, false otherwise.
 */
static bool Timer_init(void)
{
    DWT_Init(); // Initialize DWT for cycle counting
    return true; // Timer started successfully
}

/**
 * @brief Get the system tick count in milliseconds.
 * 
 * Thin wrapper around HAL_GetTick() to match the i_time_t interface.
 * 
 * @return Current tick value in ms (since system start).
 */
static uint32_t getTick(void)
{
    return HAL_GetTick();
}

/**
 * @brief Get the current System Clock frequency in Hz.
 * 
 * @return uint32_t System clock frequency in Hz.
 */
uint32_t GetSystemFrequency(void)
{
    return HAL_RCC_GetSysClockFreq();
}

/**
 * @brief Delay using DWT cycle counter (most accurate)
 * @param us Microseconds to delay
 * @note System clock must be known (e.g., 150 MHz)
 */
void DWT_delay_us(uint32_t us)
{
    uint32_t cycles = us * (SystemCoreClock / 1000000);  // Convert µs to cycles
    uint32_t start = DWT->CYCCNT;
    
    while ((DWT->CYCCNT - start) < cycles) {
        // Busy wait
    }
}

/* @brief Delay for a number of milliseconds using HAL_Delay.
 * @param ms Duration in milliseconds
 */
void HAL_Delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* -------------------------------------------------------------------------- */
/*                   Global time driver interface instance                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Time driver instance implementing the i_time_t interface.
 * 
 * Provides abstraction for system timing functions (tick, delays, frequency).
 * delay_us is not implemented and must be provided if required by the system.
 */
i_time_t driver_time = {
    .init               = Timer_init,
    .getTick            = getTick,
    .delay_ms           = HAL_Delay_ms,
    .getSystemFrequency = GetSystemFrequency,
    .delay_us           = DWT_delay_us
};

/** Global pointer to the time driver instance */
i_time_t* ITime = &driver_time;
