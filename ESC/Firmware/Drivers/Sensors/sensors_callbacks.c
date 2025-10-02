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

/* === Public Variables === */
#define ADC2_CHANNELS 2   // Number of channel in the sequence
volatile uint16_t adc2_buffer[ADC2_CHANNELS] = {0}; // DMA Buffer

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

    // 5. Start ADC1 (interrupt mode)
    HAL_ADC_Start_IT(&hadc1);

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
            MCU_Temperature_ADC_CallBack(hadc);
            break;
            
        case ADC2_BASE:
            PCB_Temperature_ADC_CallBack(adc2_buffer[PCB_SENS_VALUE]);
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
    /* Validate input parameter */
    if (hadc == NULL) {
        return;
    }
    
    /* Basic error recovery: restart ADC */
    if (hadc->Instance == ADC1) {
        /* Could add error logging here */
        HAL_ADC_Stop_IT(hadc);
        HAL_ADC_Start_IT(hadc);
    }
}

/* Future callback implementations can be added here:
 * - HAL_TIM_PeriodElapsedCallback (for sensor timing)
 * - HAL_I2C_MasterTxCpltCallback (for I2C sensors)
 * - HAL_SPI_TxRxCpltCallback (for SPI sensors)
 * etc.
 */