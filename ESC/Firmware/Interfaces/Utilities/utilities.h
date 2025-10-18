#ifndef I_DELAY_H
#define I_DELAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @file i_delay.h
 * @brief Abstract interface for delay utilities.
 *
 * Provides a hardware-agnostic API to create time delays.
 * The implementation can be based on bare-metal loops, hardware timers,
 * or vendor-specific HAL functions.
 */

/**
 * @brief Delay interface.
 */
typedef struct
{
    /**
     * @brief Initialize the delay interface.
     * @return true if initialization was successful, false otherwise
     */
    bool (*init)(void);
    /**
     * @brief Delay for a number of milliseconds.
     * @param ms Duration in milliseconds
     */
    void (*delay_ms)(uint32_t ms);

    /**
     * @brief Delay for a number of microseconds.
     * @param us Duration in microseconds
     */
    void (*delay_us)(uint32_t us);
    
    /**
     * @brief Get the current tick in milliseconds.
     * @return Current tick in ms
     */
    uint32_t (*getTick)(void); // Get current tick in ms

    /**
     * @brief Get the current frequency in Hz.
     * @return Current frequency in Hz
     */
    uint32_t (*getSystemFrequency)(void); // Get current tick in ms

} i_time_t;

/**
 * @brief Global delay interface instance.
 *
 * Defined in /config to select the active implementation.
 */
extern i_time_t* ITime;




#ifdef __cplusplus
}
#endif

#endif /* I_DELAY_H */
