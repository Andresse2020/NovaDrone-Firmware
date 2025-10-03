/**
 * @file voltage_12v.c
 * @brief Driver for 12V rail voltage sensor (STM32G4 series)
 *
 * This driver implements the i_voltage_sensor_t interface.
 * It uses ADCx (configured via CubeMX) with TIMx as trigger,
 * non-blocking, ISR-driven acquisition.
 */

#include "../sensors_callbacks.h"
#include "adc.h"

/* === External handles from CubeMX === */
extern ADC_HandleTypeDef hadc3;   // Example: ADC3 used for supply monitoring
extern TIM_HandleTypeDef htim16;  // Example: Timer triggering ADC conversions

/* === Internal cache === */
static float last_voltage = 0.0f;
static bool  valid = false;

/* === Forward declarations === */
static bool Voltage12V_Init(void);
static void Voltage12V_Reset(void);

/* === Global driver instance === */
i_voltage_sensor_t Voltage12VSensor = {
    .init  = Voltage12V_Init,
    .read  = NULL,               // Manager reads from cache only
    .reset = Voltage12V_Reset
};

/* ========================================================= */
/* === Implementation of 12V sensor functions             === */
/* ========================================================= */

/**
 * @brief Initialize the 12V rail voltage sensor.
 *        Ensures ISR-driven acquisition via ADC + timer trigger.
 */
static bool Voltage12V_Init(void)
{
    /* Reset cache */
    valid = false;
    last_voltage = 0.0f;

    return true;
}

/**
 * @brief Reset or recalibrate the 12V rail voltage sensor.
 */
static void Voltage12V_Reset(void)
{
    valid = false;
    last_voltage = 0.0f;
}

/* ========================================================= */
/* === ISR / Callback helper === */
/* ========================================================= */

/**
 * @brief Convert ADC raw value to 12V rail voltage in Volts.
 * @param adc_value Raw ADC value (0..4095 for 12-bit ADC).
 * @param vref Reference voltage in volts (e.g. 3.3).
 * @param divider_ratio Hardware resistor divider ratio (e.g. 4.0 if 47k/15k).
 * @return Voltage in Volts (float).
 */
static float Voltage12V_Convert(uint16_t adc_value, float vref, float divider_ratio)
{
    float voltage_adc = ((float)adc_value / 4095.0f) * vref;
    return voltage_adc * divider_ratio;
}

/* === Configuration constants === */
static const float Vref         = 3.3f;   // Reference voltage of ADC
static const float DividerRatio = 7.8f;   // Divider ratio ( (68k + 10k)/10k )

/**
 * @brief ADC callback for 12V rail sensor.
 * @param value Raw ADC conversion result for the 12V rail channel
 *
 * @details Converts raw ADC value into a 12V measurement and updates
 *          the voltage manager with the new sample.
 */
void Voltage12V_ADC_CallBack(uint16_t value)
{
    last_voltage = Voltage12V_Convert(value, Vref, DividerRatio);
    valid = true;

    // Notify manager with new sample
    VoltageSensorManager_OnNewSample(VOLTAGE_12V, last_voltage);
}
