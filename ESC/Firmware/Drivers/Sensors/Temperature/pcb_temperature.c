/**
 * @file pcb_temperature.c
 * @brief Driver for PCB temperature sensor (STM32G4 series)
 *
 * This driver implements the i_temperature_sensor_t interface.
 * It uses ADC2 channel 17 with TIM7 as trigger, non-blocking, ISR-driven acquisition.
 */

#include "../sensors_callbacks.h"
#include "adc.h"
#include <stdbool.h>

/* === External handles from CubeMX === */
extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim7;

/* === Internal cache === */
static float last_temperature = 0.0f;
static bool  valid = false;

/* === Forward declarations === */
static bool PCB_Init(void);
static void PCB_Update(void);
static void PCB_Calibrate(void);

/* === Global driver instance === */
i_temperature_sensor_t PCB_TemperatureSensor = {
    .init      = PCB_Init,
    .read      = NULL,
    .update    = PCB_Update,
    .calibrate = PCB_Calibrate
};

/* ========================================================= */
/* === Implementation of PCB temperature sensor functions === */
/* ========================================================= */

/**
 * @brief Initialize the PCB temperature sensor.
 *        Configures ADC2 channel 17 and ensures ISR-driven acquisition.
 */
static bool PCB_Init(void)
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
static void PCB_Update(void)
{
    // Non-blocking: TIM7 + ADC ISR updates last_temperature automatically
}

/**
 * @brief Optional calibration function for PCB temperature sensor.
 */
static void PCB_Calibrate(void)
{
    // Implement sensor-specific calibration if needed
}

/* ========================================================= */
/* === ISR / Callback helper === */
/* ========================================================= */

/**
 * @brief Convert ADC raw value to temperature in Celsius.
 * @param adc_value Raw ADC value (0..4095 for 12-bit ADC).
 * @param vref Reference voltage in volts (e.g. 3.3).
 * @return Temperature in °C (float).
 */

float Vref = 3.3;

void PCB_Temperature_Convert(uint16_t adc_value, float vref)
{
    // Convert ADC value to voltage
    float voltage = ((float)adc_value / 4095.0f) * vref;

    // Calibration points
    const float V0   = 1.90f;   // Voltage at 0 °C
    const float V80  = 2.89f;   // Voltage at 80 °C
    const float T0   = 0.0f;    // Temperature at V0
    const float T80  = 80.0f;   // Temperature at V80

    // Linear interpolation
    float temperature = ((voltage - V0) * (T80 - T0)) / (V80 - V0) + T0;

    last_temperature = temperature;
}

/**
 * @brief Callback for PCB temperature sensor
 * @param value Raw ADC conversion result for the PCB temperature channel
 *
 * @details Converts the raw ADC value into a temperature and updates
 *          the temperature manager with the new sample.
 */
void PCB_Temperature_ADC_CallBack(uint16_t value)
{
    // Convert the raw ADC value into a temperature using the PCB-specific conversion logic.
    // The conversion typically requires the ADC raw value and the reference voltage (Vref).
    PCB_Temperature_Convert(value, Vref);

    // Notify the temperature manager that a new sample is available.
    // TEMP_PCB identifies the sensor source, and last_temperature contains the latest result.
    TemperatureSensorManager_OnNewSample(TEMP_PCB, (float) last_temperature);
}

