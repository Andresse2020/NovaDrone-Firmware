#include "service_generic.h"
#include "i_time.h"


/**
 * @brief Get the system running time in seconds.
 *
 * This function retrieves the current system tick count (usually in 
 * milliseconds, depending on the HAL/OS configuration) and converts 
 * it to seconds. It is a helper for timing/debug purposes.
 *
 * @return float Running time in seconds since system start-up.
 */
float Service_getRuningTime_second(void)
{
    // Retrieve tick count (commonly in milliseconds, from system timer)
    uint32_t ticks_ms = ITime->getTick();

    // Convert milliseconds to seconds (float division for decimal precision)
    return ticks_ms / 1000.0f;
}


/**
 * @brief Convert current system tick (ms) into a formatted time string "XX h YY min ZZ sec".
 * 
 * @param buffer    Pointer to a buffer where the formatted string will be written.
 * @param buf_size  Size of the buffer in bytes.
 * 
 * @note This function calls ITime->getTick() internally.
 *       Example output: "1 h 12 min 34 sec".
 */
void Service_GetRunTimeString(char *buffer, size_t buf_size)
{
    // Retrieve current tick in milliseconds
    uint32_t tick_ms = ITime->getTick();

    // Convert to total seconds
    uint32_t total_seconds = tick_ms / 1000;

    // Split into hours, minutes, seconds
    uint32_t hours   = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t seconds = total_seconds % 60;

    // Format string safely
    snprintf(buffer, buf_size, "%lu h %lu min %lu sec",
             (unsigned long)hours,
             (unsigned long)minutes,
             (unsigned long)seconds);
}

/**
 * @brief Get the current system frequency in MHz.
 *
 * @return uint32_t System frequency in MHz.
 */
uint32_t Service_GetSysFrequencyMHz(void)
{
    // Get system frequency in Hz from ITime service
    uint32_t sys_freq_hz = ITime->getSystemFrequency();

    // Convert to MHz (integer division)
    return sys_freq_hz / 1000000U;
}

/**
 * @brief Get the current time in microseconds.
 *
 * This function retrieves the current free-running time counter
 * in microseconds from the ITime interface.
 *
 * @return uint32_t Current time in microseconds.
 */
uint32_t Service_GetTimeUs(void)
{
    return ITime->get_time_us();
}