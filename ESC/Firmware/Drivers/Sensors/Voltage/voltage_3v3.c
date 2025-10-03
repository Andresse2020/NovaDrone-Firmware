/**
 * @file voltage_3v3.c
 * @brief Driver for 3.3V rail voltage sensor (STM32G4 series)
 *
 * This driver implements the i_voltage_sensor_t interface.
 * It uses ADCx (configured via CubeMX) with TIMx as trigger,
 * non-blocking, ISR-driven acquisition.
 */

#include "../sensors_callbacks.h"
#include "adc.h"


/* === External handles from CubeMX === */
extern ADC_HandleTypeDef hadc3;   // Example: ADC3 dedicated to monitoring rails
extern TIM_HandleTypeDef htim15;  // Example: Timer triggering ADC conversions

/* === Internal cache === */
static float last_voltage = 0.0f;
static bool  valid = false;

/* === Forward declarations === */
static bool Voltage3V3_Init(void);
static void Voltage3V3_Reset(void);

/* === Global driver instance === */
i_voltage_sensor_t Voltage3V3Sensor = {
    .init  = Voltage3V3_Init,
    .read  = NULL,              // Manager will use cached value only
    .reset = Voltage3V3_Reset
};

/* ========================================================= */
/* === Implementation of 3.3V sensor functions            === */
/* ========================================================= */

/**
 * @brief Initialize the 3.3V rail voltage sensor.
 *        Ensures ISR-driven acquisition via ADC + timer trigger.
 */
static bool Voltage3V3_Init(void)
{
    /* Reset cache */
    valid = false;
    last_voltage = 0.0f;

    return true;
}

/**
 * @brief Reset or recalibrate the 3.3V rail voltage sensor.
 */
static void Voltage3V3_Reset(void)
{
    valid = false;
    last_voltage = 0.0f;
}

/* ========================================================= */
/* === ISR / Callback helper === */
/* ========================================================= */

/**
 * @brief Convert ADC raw value to 3.3V rail voltage in Volts.
 * @param adc_value Raw ADC value (0..4095 for 12-bit ADC).
 * @param vref Reference voltage in volts (e.g. 3.3).
 * @return Voltage in Volts (float).
 */
static float Voltage3V3_Convert(uint16_t adc_value, float vref, float divider_ratio)
{
    float voltage_adc = ((float)adc_value / 4095.0f) * vref;
    return voltage_adc * divider_ratio;
}

/* === Configuration constant === */
static const float Vref = 3.3f;
static const float DividerRatio  = 2.0f;  // Divider ratio ( (33k + 33k)/33k )

/**
 * @brief ADC callback for 3.3V rail sensor.
 * @param value Raw ADC conversion result for the 3.3V rail channel
 *
 * @details Converts raw ADC value into a 3.3V measurement and updates
 *          the voltage manager with the new sample.
 */
void Voltage3V3_ADC_CallBack(uint16_t value)
{
    last_voltage = Voltage3V3_Convert(value, Vref, DividerRatio);
    valid = true;

    // Notify manager with new sample
    VoltageSensorManager_OnNewSample(VOLTAGE_3V3, last_voltage);
}
