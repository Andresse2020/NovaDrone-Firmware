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
#include "i_motor_sensor.h"


/* ==============================       Public Variables        ============================== */

/**
 * @brief ADC1 channel mapping
 *
 * Defines the order of channels in the ADC1 conversion sequence.
 * Each entry corresponds to a physical signal connected to ADC1.
 */
typedef enum {
    V12_SENS_VALUE = 0,   /**< 12V rail voltage sensor */
} ADC3_channel_t;


/**
 * @brief ADC2 channel mapping
 *
 * Defines the order of channels in the ADC2 conversion sequence.
 */
typedef enum {
    V3v3_SENS_VALUE,      /**< 3.3V rail voltage sensor */
    VBUS_SENS_VALUE       /**< Bus voltage sensor */
} ADC4_channel_t;


/**
 * @brief ADC3 channel mapping
 *
 * Defines the order of channels in the ADC3 conversion sequence.
 */
typedef enum {
    MCU_SENS_VALUE = 0,   /**< Internal MCU temperature sensor */
} ADC5_channel_t;


/* ==============================       Configuration Constants        ============================== */

#define ADC3_CHANNELS 1   /**< Number of channels in ADC1 regular sequence */
#define ADC4_CHANNELS 2   /**< Number of channels in ADC2 regular sequence */
#define ADC5_CHANNELS 1   /**< Number of channels in ADC3 regular sequence */


/* ==============================       Shared Buffers (DMA)        ============================== */

/**
 * @brief DMA buffers for ADC data acquisition
 *
 * Each buffer holds the most recent ADC conversion results for the corresponding ADC instance.
 * Declared as volatile since they are updated by DMA in the background.
 */
extern volatile uint16_t adc3_buffer[ADC3_CHANNELS];
extern volatile uint16_t adc4_buffer[ADC4_CHANNELS];
extern volatile uint16_t adc5_buffer[ADC5_CHANNELS];

/*
    * @brief Shared buffer for motor measurements
    *
    * This buffer is updated by the ADC ISR and read by the FOC loop.
    * Declared as extern to allow access from other modules.
*/
extern volatile motor_measurements_t adc_motor_measurement_buffer;


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
 * @brief Notify temperature sensor manager of ADC5 end-of-conversion block
 *
 * @details Called when ADC1 completes a block of conversions related
 *          to temperature sensors. Triggers post-processing or data update.
 */
void TemperatureSensorManager_OnEndOfBlock_ADC5(void);

/**
 * @brief Notify voltage sensor manager of ADC1 end-of-conversion block
 *
 * @details Called when ADC3 completes a conversion sequence for voltage sensors.
 */
void VoltageSensorManager_OnEndOfBlock_ADC3(void);

/**
 * @brief Notify voltage sensor manager of ADC4 end-of-conversion block
 *
 * @details Called when ADC2 completes a conversion sequence for voltage sensors.
 */
void VoltageSensorManager_OnEndOfBlock_ADC4(void);

/**
 * @brief Notify the motor sensor manager that new ADC data is ready
 *
 * This function should be called by the ADC ISR after updating
 * `adc_motor_measurement_buffer` to inform the FOC loop that
 * new measurements are available.
 */
void adc_notify_new_data_ready(void);



#ifdef __cplusplus
}
#endif

#endif /* SENSORS_CALLBACKS_H */