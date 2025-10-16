/**
 * @file i_motor_sensor.c
 * @brief Implementation of the ADC measurement interface for FOC control.
 *
 * The ADC ISR fills `adc_motor_measurement_buffer`.
 * This interface exposes a read-only accessor for the Control Layer (FOC),
 * guaranteeing clean and deterministic access to the latest measurements.
 */

#include "../sensors_callbacks.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                             Internal state flag                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Indicates whether new measurements are available
 * since the last FOC read cycle.
 * The Service Layer must set this flag to true after each update.
 */
static volatile bool is_new_data_ready = false;

/* -------------------------------------------------------------------------- */
/*                       Interface function implementation                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Retrieves all the latest raw ADC measurements for FOC.
 * @note  Must be called from the 24 kHz FOC timer interrupt.
 */
static bool get_latest_measurements_impl(motor_measurements_t *meas)
{
    if (!is_new_data_ready)
        return false;

    // Atomic read: one single memcpy from shared buffer.
    memcpy(meas, (const void*)&adc_motor_measurement_buffer, sizeof(motor_measurements_t));

    is_new_data_ready = false;
    return true;
}

/* -------------------------------------------------------------------------- */
/*                           Interface registration                           */
/* -------------------------------------------------------------------------- */

static i_motor_sensor_t s_adc_interface = {
    .get_latest_measurements = get_latest_measurements_impl
};

i_motor_sensor_t* IMotor_ADC_Measure = &s_adc_interface;

/* -------------------------------------------------------------------------- */
/*                    API for the Service Layer (optional)                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Called by the Service Layer when a new ADC sample set is ready.
 * @note  Allows the FOC loop to know when to fetch data.
 */
void adc_notify_new_data_ready(void)
{
    is_new_data_ready = true;
}
