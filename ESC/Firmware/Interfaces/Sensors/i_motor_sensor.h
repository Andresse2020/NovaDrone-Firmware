/**
 * @file i_adc.h
 * @brief Interface for critical measurement acquisition for FOC control.
 *
 * This file defines the interface between the Service Layer (ADC management)
 * and the Control Layer (FOC algorithm). It ensures deterministic and 
 * atomic access to the latest raw measurements.
 */

#ifndef I_ADC_H
#define I_ADC_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Structure holding the raw ADC values for the six critical measurements.
 * * Raw values (raw counts, 0-4095) are used to minimize the time 
 * of the ADC's Interrupt Service Routine (ISR). The conversion to engineering 
 * units (A, V) must be performed by the FOC loop (fast_loop) itself.
 */
typedef struct {
    // Current Measurements (shunt)
    uint16_t i_a_raw;       /**< Phase A current (raw ADC value) */
    uint16_t i_b_raw;       /**< Phase B current (raw ADC value) */
    uint16_t i_c_raw;       /**< Phase C current (raw ADC value) */
    
    // Phase Voltage Measurements (for BEMF Estimation)
    uint16_t v_phase_a_raw; /**< Phase A voltage (raw ADC value) */
    uint16_t v_phase_b_raw; /**< Phase B voltage (raw ADC value) */
    uint16_t v_phase_c_raw; /**< Phase C voltage (raw ADC value) */

} motor_measurements_t;


/**
 * @brief ADC Interface for the Control Layer.
 * * Provides access via a single function to guarantee coherence 
 * of the 6 critical signals (atomic read).
 */
typedef struct {
    /**
     * @brief Retrieves all the latest critical measurements (Phase I and V).
     * * This function is designed to be called by the FOC Timer's ISR. 
     * It reads all data in a single operation for atomicity.
     * * @param meas Pointer to the structure to be filled with raw values.
     * @return true if new data is available since the last read, false otherwise.
     */
    bool (*get_latest_measurements)(motor_measurements_t *meas);

} i_motor_sensor_t;


/**
 * @brief Accessor for the ADC interface.
 * * Allows the Control Layer (FOC) to get the instance of the interface.
 * * @return Pointer to the active ADC interface.
 */
extern i_motor_sensor_t* IMotor_ADC_Measure;


// -----------------------------------------------------------------------------
// Important Note for the Control Layer:
// The FOC must first check the data availability flag before calling 
// get_latest_measurements.
// -----------------------------------------------------------------------------

#endif // I_ADC_H