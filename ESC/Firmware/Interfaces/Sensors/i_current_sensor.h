#ifndef I_CURRENT_SENSOR_H
#define I_CURRENT_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @file i_current_sensor.h
 * @brief Abstract interface for motor current sensors.
 *
 * Provides a hardware-agnostic API to read motor phase currents
 * required by the ESC algorithm.
 */

/**
 * @brief Current sensor identifiers (for multi-phase motors)
 */
typedef enum {
    CURRENT_PHASE_A,
    CURRENT_PHASE_B,
    CURRENT_PHASE_C
} current_sensor_id_t;

/**
 * @brief Current measurement structure.
 */
typedef struct {
    float phase_a;   /**< Current in phase A [A] */
    float phase_b;   /**< Current in phase B [A] */
    float phase_c;   /**< Current in phase C [A] */
} current_measurement_t;

/**
 * @brief Current sensor interface.
 */
typedef struct
{
    /**
     * @brief Initialize current sensors.
     * @return true if successful, false otherwise
     */
    bool (*init)(void);

    /**
     * @brief Read current of a specific phase.
     * @param id Phase identifier
     * @param current Pointer to store measured current [A]
     * @return true if successful, false otherwise
     */
    bool (*read_phase)(current_sensor_id_t id, float* current);

    /**
     * @brief Read all three phase currents at once.
     * @param measurement Pointer to store measured currents
     * @return true if successful, false otherwise
     */
    bool (*read_all)(current_measurement_t* measurement);

    /**
     * @brief Optional calibration/reset function
     */
    void (*reset)(void);

} i_current_sensor_t;

extern i_current_sensor_t* ICurrentSensor;

#ifdef __cplusplus
}
#endif

#endif /* I_CURRENT_SENSOR_H */
