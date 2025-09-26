// Interfaces/Include/interfaces_init.h
#ifndef INTERFACES_INIT_H
#define INTERFACES_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum
{
    I_OK = 0,       // Operation successful
    I_ERROR,       // General error 
} i_status_t;


/**
 * @brief Initialize system core (HAL, clock)
 *
 * This function should be called once at system startup.
 * It initializes HAL and system clock.
 *
 * @return true if all initializations succeed, false otherwise
 */
i_status_t DSystem_Init(void);


/**
 * @brief Initialize essential drivers and peripherals
 *
 * This function initializes GPIOs and essential interfaces
 * like UART, FDCAN, etc.
 *
 * @return true if all initializations succeed, false otherwise
 */
i_status_t Driver_Init(void);

/**
 * @brief Perform a software reset of the MCU.
 */
void DSystem_Reset(void);

float Driver_MCU_GetTemperature(void);

#ifdef __cplusplus
}
#endif

#endif /* INTERFACES_INIT_H */
