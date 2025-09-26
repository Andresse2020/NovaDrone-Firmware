/**
 * @file sensors_callbacks.h
 * @brief Header for sensor callback dispatcher module
 * @author BASSINGA Brisel
 * @date 25/11/2025
 * 
 * This module provides centralized HAL callback handling for all sensor-related
 * hardware interrupts. It ensures clean separation between hardware abstraction
 * and sensor driver logic.
 */

#ifndef SENSORS_CALLBACKS_H
#define SENSORS_CALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "bsp_utils.h"
#include "i_temperature_sensor.h"  /* For temperature_sensor_id_t */


/* ==============================   Public Function Prototypes  ============================== */

/**
 * @brief Initialize the sensors callback system
 * @details Must be called during system initialization to ensure proper linkage
 * @retval true  Initialization successful
 * @retval false Initialization failed or already initialized
 */
bool SensorsCallbacks_Init(void);

/**
 * @brief Check if callbacks system is initialized
 * @retval true  Callbacks are active
 * @retval false Callbacks not initialized
 */
bool SensorsCallbacks_IsInitialized(void);

/* ==============================   Sensor Interface Functions  ============================== */

/**
 * @brief Temperature sensor manager callback for new sample data
 * @param id Temperature sensor identifier
 * @param value Measured temperature value in degrees Celsius
 * @details Called by sensor drivers to update the central temperature cache
 */
void TemperatureSensorManager_OnNewSample(temperature_sensor_id_t id, float value);

/**
 * @brief Temperature sensor ADC conversion callback
 * @param hadc ADC handle pointer from temperature sensor
 * @details Processes ADC conversion results for temperature measurements
 */
void MCU_Temperature_ADC_CallBack(ADC_HandleTypeDef* hadc);


#ifdef __cplusplus
}
#endif

#endif /* SENSORS_CALLBACKS_H */