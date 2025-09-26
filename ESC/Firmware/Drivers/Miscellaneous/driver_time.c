#include "utilities.h"
#include "bsp_utils.h"

/* -------------------------------------------------------------------------- */
/*                   Local static helper function definitions                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Delay for a specified time in milliseconds.
 * 
 * Thin wrapper around HAL_Delay() to match the i_time_t interface.
 * 
 * @param ms Duration to wait, in milliseconds.
 */
static void hal_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
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
    .getTick            = getTick,
    .delay_ms           = hal_delay_ms,
    .getSystemFrequency = GetSystemFrequency,
    .delay_us           = NULL // Not implemented (requires timer-based approach)
};

/** Global pointer to the time driver instance */
i_time_t* ITime = &driver_time;
