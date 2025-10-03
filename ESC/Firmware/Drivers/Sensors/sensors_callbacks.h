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
#include "i_voltage_sensor.h"


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
void MCU_Temperature_ADC_CallBack(uint16_t value);

/**
 * @brief Callback for PCB temperature sensor
 * @param value Raw ADC value of the PCB temperature channel
 *
 * @details Called after ADC conversion to process the PCB temperature measurement.
 *          Typically used to convert the raw value into °C and update monitoring logic.
 */
void PCB_Temperature_ADC_CallBack(uint16_t value);

/**
 * @brief Called by ISR or ADC callback to store a new voltage sample in cache.
 * @param id Voltage sensor identifier
 * @param value Measured voltage value [V]
 */
void VoltageSensorManager_OnNewSample(voltage_sensor_id_t id, float value);

/**
 * @brief ADC callback for Bus Voltage sensor.
 * @param value Raw ADC conversion result for the Bus Voltage channel
 *
 * @details Converts raw ADC value into a bus voltage and updates
 *          the voltage manager with the new sample.
 */
void Voltage_Bus_ADC_CallBack(uint16_t value);

/**
 * @brief ADC callback for 3.3V rail sensor.
 * @param value Raw ADC conversion result for the 3.3V rail channel
 *
 * @details Converts raw ADC value into a 3.3V measurement and updates
 *          the voltage manager with the new sample.
 */
void Voltage3V3_ADC_CallBack(uint16_t value);

/**
 * @brief ADC callback for 12V rail sensor.
 * @param value Raw ADC conversion result for the 12V rail channel
 *
 * @details Converts raw ADC value into a 12V measurement and updates
 *          the voltage manager with the new sample.
 */
void Voltage12V_ADC_CallBack(uint16_t value);


#ifdef __cplusplus
}
#endif

#endif /* SENSORS_CALLBACKS_H */