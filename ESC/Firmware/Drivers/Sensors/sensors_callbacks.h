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


/* ==============================       Public Variables        ============================== */

/* =========================================================
 * === ADC End-Of-Block Flags
 * ========================================================= */

typedef enum{
    MCU_SENS_VALUE = 0,
    V3v3_SENS_VALUE,
}ADC1_channel_t;

typedef enum{
    PCB_SENS_VALUE = 0,
    VBUS_SENS_VALUE
}ADC2_channel_t;

typedef enum{
    V12_SENS_VALUE = 0,
}ADC3_channel_t;


#define ADC1_CHANNELS 3   // Number of channel in the sequence
#define ADC2_CHANNELS 3   // Number of channel in the sequence
#define ADC3_CHANNELS 2   // Number of channel in the sequence

/* =========================================================
 * === External DMA Buffers
 * ========================================================= */
extern volatile uint16_t adc1_buffer[ADC1_CHANNELS];
extern volatile uint16_t adc2_buffer[ADC2_CHANNELS];
extern volatile uint16_t adc3_buffer[ADC3_CHANNELS];


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

void TemperatureSensorManager_OnEndOfBlock_ADC1(void);
void TemperatureSensorManager_OnEndOfBlock_ADC2(void);

void VoltageSensorManager_OnEndOfBlock_ADC1(void);
void VoltageSensorManager_OnEndOfBlock_ADC2(void);
void VoltageSensorManager_OnEndOfBlock_ADC3(void); 


#ifdef __cplusplus
}
#endif

#endif /* SENSORS_CALLBACKS_H */