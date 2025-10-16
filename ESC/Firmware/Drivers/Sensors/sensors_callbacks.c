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
#include "usart.h"

/* === Private Constants === */
#define SENSORS_CALLBACKS_VERSION_MAJOR    1
#define SENSORS_CALLBACKS_VERSION_MINOR    0

/* === Private Variables === */
static bool callbacks_initialized = false;

// Local buffer updated by the ADC ISR (Service Layer). 
volatile motor_measurements_t adc_motor_measurement_buffer;

volatile uint16_t adc3_buffer[ADC3_CHANNELS] = {0}; // DMA Buffer
volatile uint16_t adc4_buffer[ADC4_CHANNELS] = {0}; // DMA Buffer
volatile uint16_t adc5_buffer[ADC5_CHANNELS] = {0}; // DMA Buffer

/* ============================================================================ */
/* === Public Functions                                                     === */
/* ============================================================================ */

/**
 * @brief Enable DWT cycle counter for performance measurement
 * @note Must be called once during initialization
 */
void DWT_Init(void)
{
    // Enable TRC (Trace)
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    // Reset cycle counter
    DWT->CYCCNT = 0;
    
    // Enable cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

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

    // -------------------------------------------------------------------------
    // 1 Calibrate ADCs (ADC1, ADC3)
    // -------------------------------------------------------------------------
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();
    if (HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK) Error_Handler();


    // Datasheet recommends a small delay after calibration
    HAL_Delay(5); 

    // -------------------------------------------------------------------------
    // 2 Configure TIM1 TRGO to trigger injected ADC conversions
    // -------------------------------------------------------------------------

    HAL_TIM_OC_Start(&htim1, TIM_CHANNEL_4);

    // -------------------------------------------------------------------------
    // 3 Configure TIM6 TRGO to trigger regular ADC conversions via DMA
    // -------------------------------------------------------------------------

    HAL_TIM_Base_Start(&htim6);

    // -------------------------------------------------------------------------
    // 4 Start regular ADC conversions via DMA
    // -------------------------------------------------------------------------
    
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*) adc3_buffer, ADC3_CHANNELS) != HAL_OK) 
        Error_Handler();

    if (HAL_ADC_Start_DMA(&hadc4, (uint32_t*) adc4_buffer, ADC4_CHANNELS) != HAL_OK) 
        Error_Handler();

    if (HAL_ADC_Start_DMA(&hadc5, (uint32_t*) adc5_buffer, ADC5_CHANNELS) != HAL_OK) 
        Error_Handler();

    // -------------------------------------------------------------------------
    // 5 Start injected ADC conversions in interrupt mode (synchronized)
    // -------------------------------------------------------------------------
    // AutoInjectedConv = DISABLE because TRGO triggers conversions
    if (HAL_ADCEx_InjectedStart_IT(&hadc1) != HAL_OK) Error_Handler();

    HAL_Delay(5); // Small delay to ensure ADCs are stable
    
    if (HAL_ADCEx_InjectedStart_IT(&hadc2) != HAL_OK) Error_Handler();

    // -------------------------------------------------------------------------
    // 6 Additional timers or slow-loop tasks can be started here
    // -------------------------------------------------------------------------
    // HAL_TIM_Base_Start(&htim6);

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
        case ADC3_BASE:
            VoltageSensorManager_OnEndOfBlock_ADC3();
            break;
            
        case ADC4_BASE:
            VoltageSensorManager_OnEndOfBlock_ADC4();
            break;

        case ADC5_BASE:
            TemperatureSensorManager_OnEndOfBlock_ADC5();
            break;
            
        default:
            /* Unexpected ADC instance - could log error if needed */
            break;
    }
}

/**
 * @brief Injected Conversion Complete Callback (Called by the HAL_ADC_IRQHandler).
 * * This ISR is triggered by ADC1 (Master) at 24 kHz after both conversions are done.
 * * It copies the data from ADC JDRs to the buffer and sets the synchronization flag.
 */
#define IIR_ALPHA 5  // Coefficient de filtrage (2^n pour optimisation)

void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{    
    if (hadc->Instance != ADC1) return;
    
    static uint32_t i_a_filt = 0;
    static uint32_t i_b_filt = 0;
    static bool first_run = true;
    
    uint16_t i_a_raw = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);
    uint16_t i_b_raw = HAL_ADCEx_InjectedGetValue(&hadc2, ADC_INJECTED_RANK_1);
    
    if (first_run) {
        // Initialisation
        i_a_filt = (uint32_t)i_a_raw << IIR_ALPHA;
        i_b_filt = (uint32_t)i_b_raw << IIR_ALPHA;
        first_run = false;
    }
    
    // Filtre IIR: y[n] = (15/16)*y[n-1] + (1/16)*x[n]
    i_a_filt = i_a_filt - (i_a_filt >> IIR_ALPHA) + i_a_raw;
    i_b_filt = i_b_filt - (i_b_filt >> IIR_ALPHA) + i_b_raw;
    
    adc_motor_measurement_buffer.i_a_raw = (uint16_t)(i_a_filt >> IIR_ALPHA);
    adc_motor_measurement_buffer.i_b_raw = (uint16_t)(i_b_filt >> IIR_ALPHA);

    adc_notify_new_data_ready();
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
        HAL_ADC_Stop_DMA(hadc);
    }
    
}

/* Future callback implementations can be added here:
 * - HAL_TIM_PeriodElapsedCallback (for sensor timing)
 * - HAL_I2C_MasterTxCpltCallback (for I2C sensors)
 * - HAL_SPI_TxRxCpltCallback (for SPI sensors)
 * etc.
 */