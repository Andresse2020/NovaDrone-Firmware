#ifndef I_VOLTAGE_SENSOR_H
#define I_VOLTAGE_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @file i_voltage_sensor.h
 * @brief Abstract interface for voltage sensors (DC bus, battery, etc.).
 *
 * Provides a hardware-agnostic API to read voltage values needed by ESC algorithms
 * and safety protections.
 */

/**
 * @brief Voltage sensor identifiers.
 */
typedef enum {
    VOLTAGE_BUS,       /**< DC bus voltage */
    VOLTAGE_3V3,
    VOLTAGE_12V,
    VOLT_SENSOR_COUNT
} voltage_sensor_id_t;

/**
 * @brief Voltage sensor interface.
 */
typedef struct
{
    /**
     * @brief Initialize voltage sensors.
     * @return true if successful, false otherwise
     */
    bool (*init)(void);

    /**
     * @brief Periodic non-blocking update function.
     * Should trigger new acquisition or refresh cached value.
     */
    void (*update)(void);

    /**
     * @brief Read voltage of a specific sensor.
     * @param id Voltage sensor identifier
     * @param voltage Pointer to store measured voltage [V]
     * @return true if successful, false otherwise
     */
    bool (*read)(voltage_sensor_id_t id, float* voltage);

    /**
     * @brief Optional calibration / zero reference function
     */
    void (*reset)(void);

} i_voltage_sensor_t;

extern i_voltage_sensor_t* IVoltageSensor;

#ifdef __cplusplus
}
#endif

#endif /* I_VOLTAGE_SENSOR_H */