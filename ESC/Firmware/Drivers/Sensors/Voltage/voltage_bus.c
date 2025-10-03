/**
 * @file bus_voltage.c
 * @brief Driver for DC Bus Voltage sensor (STM32G4 series)
 *
 * This driver implements the i_voltage_sensor_t interface.
 * It uses ADC1 (for example channel X configured in CubeMX) with TIM6 as trigger,
 * non-blocking, ISR-driven acquisition.
 */

#include "../sensors_callbacks.h"
#include "adc.h"

/* === External handles from CubeMX === */
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim6;

/* === Internal cache === */
static float last_voltage = 0.0f;
static bool  valid = false;

/* === Forward declarations === */
static bool VoltageBus_Init(void);
static void VoltageBus_Reset(void);

/* === Global driver instance === */
i_voltage_sensor_t VoltageBusSensor = {
    .init  = VoltageBus_Init,
    .read  = NULL,
    .reset = VoltageBus_Reset
};

/* ========================================================= */
/* === Implementation of Bus Voltage sensor functions     === */
/* ========================================================= */

/**
 * @brief Initialize the Bus Voltage sensor.
 *        Configures ADC1 (channel for DC Bus) and ensures ISR-driven acquisition.
 */
static bool VoltageBus_Init(void)
{
    /* Reset cache */
    valid = false;
    last_voltage = 0.0f;

    return true;
}

/**
 * @brief Reset or recalibrate the Bus Voltage sensor.
 */
static void VoltageBus_Reset(void)
{
    valid = false;
    last_voltage = 0.0f;
}

/* ========================================================= */
/* === ISR / Callback helper === */
/* ========================================================= */

/**
 * @brief Convert ADC raw value to bus voltage in Volts.
 * @param adc_value Raw ADC value (0..4095 for 12-bit ADC).
 * @param vref Reference voltage in volts (e.g. 3.3).
 * @param divider_ratio Hardware resistor divider ratio (e.g. 11.0 if 100k/10k divider).
 * @return Bus voltage in Volts (float).
 */
static float VoltageBus_Convert(uint16_t adc_value, float vref, float divider_ratio)
{
    float voltage_adc = ((float)adc_value / 4095.0f) * vref;
    return voltage_adc * divider_ratio;
}

/* === Configuration constants === */
static const float Vref          = 3.3f;   // Reference voltage
static const float DividerRatio  = 11.0f;  // Divider ratio ( (180k + 18k)/18k )

/**
 * @brief ADC callback for Bus Voltage sensor.
 * @param value Raw ADC conversion result for the Bus Voltage channel
 *
 * @details Converts raw ADC value into a bus voltage and updates
 *          the voltage manager with the new sample.
 */
void Voltage_Bus_ADC_CallBack(uint16_t value)
{
    last_voltage = VoltageBus_Convert(value, Vref, DividerRatio);
    valid = true;

    // Notify manager with new sample
    VoltageSensorManager_OnNewSample(VOLTAGE_BUS, last_voltage);
}
