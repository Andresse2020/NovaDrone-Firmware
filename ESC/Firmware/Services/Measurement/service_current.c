#include "services.h"
#include "i_motor_sensor.h"

/* -------------------------------------------------------------------------- */
/*                        Internal service context                            */
/* -------------------------------------------------------------------------- */

/** Internal buffer updated by Service_ADC_UpdateMeasurements() */
static motor_measurements_t s_motor_meas;

/** Flag to indicate that the buffer holds fresh data */
static bool s_measurements_valid = false;

/* -------------------------------------------------------------------------- */
/*                          Public Service Functions                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Updates the local measurement buffer with the latest ADC readings.
 *
 * This function must be called periodically by the Control Layer (for example,
 * inside the 50 kHz FOC timer ISR) to fetch the most recent synchronized ADC
 * measurements from the i_adc interface.
 *
 * @return true if new data was fetched successfully, false otherwise.
 */
bool Service_ADC_Motor_UpdateMeasurements(void)
{
    bool ok = IMotor_ADC_Measure->get_latest_measurements(&s_motor_meas);
    s_measurements_valid = ok;
    return ok;
}

/**
 * @brief Returns the most recent measured Phase A current in amperes.
 * @return float Phase A current (A), or 0.0f if no valid measurement available.
 */
float Service_Get_PhaseA_Current(void)
{
    if (!s_measurements_valid)
        return 0.0f;

    return Service_ADC_To_Current(s_motor_meas.i_a_raw);
}

/**
 * @brief Returns the most recent measured Phase B current in amperes.
 * @return float Phase B current (A), or 0.0f if no valid measurement available.
 */
float Service_Get_PhaseB_Current(void)
{
    if (!s_measurements_valid)
        return 0.0f;

    return Service_ADC_To_Current(s_motor_meas.i_b_raw);
}

/**
 * @brief Returns the most recent measured Phase B current in amperes.
 * @return float Phase C current (A), or 0.0f if no valid measurement available.
 */
float Service_Get_PhaseC_Current(void)
{
    if (!s_measurements_valid)
        return 0.0f;

    return Service_ADC_To_Current(s_motor_meas.i_c_raw);
}