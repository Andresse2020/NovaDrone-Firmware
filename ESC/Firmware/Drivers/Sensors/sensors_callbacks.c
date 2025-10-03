/**
 * @file sensors_callbacks.c
 * @brief Central HAL callback dispatcher for all sensor-related interrupts
 * @author BASSINGA Brisel
 * @date 25/09/2025
 * 
 * This module centralizes all HAL callbacks related to sensor data acquisition.
 * It acts as a dispatcher, routing hardware interrupts to appropriate sensor drivers.
 * 
 * @note This file must be explicitly linked by calling SensorsCallbacks_Init()
 *       to ensure the callback functions override the weak HAL implementations.
 */

#include "sensors_callbacks.h"
#include "adc.h"
#include "gpio.h"
#include "tim.h"

/* === Private Constants === */
#define SENSORS_CALLBACKS_VERSION_MAJOR    1
#define SENSORS_CALLBACKS_VERSION_MINOR    0

/* === Private Variables === */
static bool callbacks_initialized = false;

typedef enum{
    PCB_SENS_VALUE = 0,
    VBUS_VALUE
}ADC2_channel_t;

typedef enum{
    MCU_SENS_VALUE = 0,
    V3v3_VALUE,
}ADC1_channel_t;

/* === Public Variables === */
#define ADC2_CHANNELS 2   // Number of channel in the sequence
volatile uint16_t adc2_buffer[ADC2_CHANNELS] = {0}; // DMA Buffer

#define ADC1_CHANNELS 2   // Number of channel in the sequence
volatile uint16_t adc1_buffer[ADC1_CHANNELS] = {0};

/* ============================================================================ */
/* === Public Functions                                                     === */
/* ============================================================================ */

/**
 * @brief Initialize the sensors callback system
 * @details This function must be called during system initialization to ensure
 *          that this file is properly linked and the callbacks are active.
 * @retval true  Initialization successful
 * @retval false Initialization failed or already initialized
 */
bool SensorsCallbacks_Init(void)
{
    if (callbacks_initialized) {
        return false;
    }

    // 1. Calibrate ADC1 and ADC2
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }
    
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
        Error_Handler();
    }

    // 2. Start the timer BEFORE starting the ADCs
    HAL_TIM_Base_Start(&htim7);
    
    // 3. Short delay to stabilize the timer
    HAL_Delay(10);

    // 4. Start ADC2 with DMA
    if (HAL_ADC_Start_DMA(&hadc2, (uint32_t*) adc2_buffer, ADC2_CHANNELS) != HAL_OK) {
        Error_Handler();
    }

    HAL_Delay(10);

    // 5. Start ADC1 with DMA
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc1_buffer, ADC1_CHANNELS) != HAL_OK) {
        Error_Handler();
    }

    // 6. Start ADC3 in interrupt mode
    HAL_ADC_Start_IT(&hadc3);

    callbacks_initialized = true;
    return true;
}

/**
 * @brief Get initialization status of callbacks system
 * @retval true  Callbacks are initialized and active
 * @retval false Callbacks not initialized
 */
bool SensorsCallbacks_IsInitialized(void)
{
    return callbacks_initialized;
}

/* ============================================================================ */
/* === HAL Callback Implementations                                         === */
/* ============================================================================ */

/**
 * @brief HAL ADC conversion complete callback dispatcher
 * @param hadc Pointer to ADC handle that triggered the interrupt
 * @details Routes ADC completion events to appropriate sensor drivers:
 *          - ADC1: MCU internal temperature sensor
 *          - ADC3: External voltage/current sensors (future)
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    /* Validate input parameter */
    if (hadc == NULL) {
        return;
    }
    
    /* Route to appropriate sensor driver based on ADC instance */
    switch ((uint32_t)hadc->Instance) {
        case ADC1_BASE:
            MCU_Temperature_ADC_CallBack(adc1_buffer[MCU_SENS_VALUE]);
            Voltage3V3_ADC_CallBack(adc1_buffer[V3v3_VALUE]);
            break;
            
        case ADC2_BASE:
            PCB_Temperature_ADC_CallBack(adc2_buffer[PCB_SENS_VALUE]);
            Voltage_Bus_ADC_CallBack(adc2_buffer[VBUS_VALUE]);
            break;

        case ADC3_BASE:
            /* Get raw ADC conversion result */
            uint16_t raw_value = (uint16_t)HAL_ADC_GetValue(hadc);
            Voltage12V_ADC_CallBack(raw_value);
            break;
            
        default:
            /* Unexpected ADC instance - could log error if needed */
            break;
    }
}

/**
 * @brief HAL ADC error callback dispatcher
 * @param hadc Pointer to ADC handle that generated the error
 * @note Currently handles errors by resetting the ADC - could be enhanced
 *       with proper error logging and recovery strategies
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == NULL) {
        return;
    }
    
    /* Restart ADC with proper DMA mode */
    if (hadc->Instance == ADC1) {
        HAL_ADC_Stop_DMA(hadc);
    }
    else if (hadc->Instance == ADC2) {
        HAL_ADC_Stop_DMA(hadc);
    }
    else if (hadc->Instance == ADC3)
    {
        HAL_ADC_Stop_IT(&hadc3);
    }
    
}

/* Future callback implementations can be added here:
 * - HAL_TIM_PeriodElapsedCallback (for sensor timing)
 * - HAL_I2C_MasterTxCpltCallback (for I2C sensors)
 * - HAL_SPI_TxRxCpltCallback (for SPI sensors)
 * etc.
 */