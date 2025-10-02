/**
 * @file mcu_temperature_sensor.c
 * @brief Driver for MCU internal temperature sensor (STM32G4 series)
 *
 * This driver implements the i_temperature_sensor_t interface.
 * It uses ADC1 with TIM7 as trigger, non-blocking, ISR-driven acquisition.
 */

#include "../sensors_callbacks.h"
#include "adc.h"
#include <stdbool.h>

/* === Internal cache === */
static float last_temperature = 0.0f;
static bool  valid = false;

/* === Forward declarations === */
static bool MCU_Init(void);
static void MCU_Update(void);
static void MCU_Calibrate(void);

/* === Global driver instance === */
i_temperature_sensor_t MCU_TemperatureSensor = {
    .init      = MCU_Init,
    .read      = NULL,
    .update    = MCU_Update,
    .calibrate = MCU_Calibrate
};

/* ========================================================= */
/* === Implementation of MCU temperature sensor functions === */
/* ========================================================= */

/**
 * @brief Initialize the MCU internal temperature sensor.
 *        Configures ADC1 internal channel (TempSensor + VREFINT)
 *        and ensures the ADC is ready for ISR-driven acquisition.
 */
static bool MCU_Init(void)
{
    /* Reset cache */
    valid = false;
    last_temperature = 0.0f;

    return true;
}

/**
 * @brief Triggered by Manager update.
 *        Acquisition is ISR-driven, so nothing to do.
 */
static void MCU_Update(void)
{
    // Non-blocking: TIM7 + ADC ISR updates last_temperature automatically
}

/**
 * @brief Optional calibration function for MCU temperature sensor.
 */
static void MCU_Calibrate(void)
{
    // Could implement factory calibration offset if needed
}

/* ========================================================= */
/* === ISR / Callback helper === */
/* ========================================================= */

/**
 * @brief Called by HAL_ADC_ConvCpltCallback for ADC1 internal TempSensor.
 *        Converts raw ADC value to temperature and updates cache.
 * @param raw_adc Raw 12-bit ADC value from ADC1
 */
void MCU_TemperatureSensor_OnNewSample(uint32_t raw_adc)
{
    /* Convert raw ADC value to temperature [°C] using STM32 macro */
    /* Ensure VDDA is correct (mV). Typical 3.3V -> 3300 mV */
    float temp_c = __HAL_ADC_CALC_TEMPERATURE(3300, raw_adc, ADC_RESOLUTION_12B);

    last_temperature = temp_c;
    valid = true;
}

/**
 * @brief HAL ADC conversion complete callback
 */
void MCU_Temperature_ADC_CallBack(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;

    // Read ADC value
    uint32_t raw = HAL_ADC_GetValue(hadc);

    // Call MCU driver
    MCU_TemperatureSensor_OnNewSample(raw);

    // Update manager cache
    TemperatureSensorManager_OnNewSample(TEMP_MCU, last_temperature);
}


