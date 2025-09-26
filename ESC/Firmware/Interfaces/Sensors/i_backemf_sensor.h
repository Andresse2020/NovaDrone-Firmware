#ifndef I_BACKEMF_SENSOR_H
#define I_BACKEMF_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @file i_backemf_sensor.h
 * @brief Abstract interface for estimating motor voltage (Back-EMF) in sensorless ESCs.
 *
 * Provides a hardware-agnostic API for reading estimated motor phase voltages
 * needed by sensorless control algorithms.
 */

/**
 * @brief Motor phase identifiers.
 */
typedef enum {
    PHASE_A,
    PHASE_B,
    PHASE_C
} backemf_phase_t;

/**
 * @brief Estimated back-EMF voltages structure.
 */
typedef struct {
    float phase_a; /**< Estimated voltage on phase A [V] */
    float phase_b; /**< Estimated voltage on phase B [V] */
    float phase_c; /**< Estimated voltage on phase C [V] */
} backemf_measurement_t;

/**
 * @brief Back-EMF sensor interface.
 */
typedef struct
{
    /**
     * @brief Initialize back-EMF estimation module.
     * @return true if successful, false otherwise
     */
    bool (*init)(void);

    /**
     * @brief Read estimated voltage of a specific phase.
     * @param phase Phase identifier
     * @param voltage Pointer to store estimated voltage [V]
     * @return true if successful, false otherwise
     */
    bool (*read_phase)(backemf_phase_t phase, float* voltage);

    /**
     * @brief Read estimated voltages for all three phases.
     * @param measurement Pointer to store phase voltages
     * @return true if successful, false otherwise
     */
    bool (*read_all)(backemf_measurement_t* measurement);

    /**
     * @brief Optional reset or recalibration of estimation
     */
    void (*reset)(void);

} i_backemf_sensor_t;

extern i_backemf_sensor_t* IBackEMFSensor;

#ifdef __cplusplus
}
#endif

#endif /* I_BACKEMF_SENSOR_H */
