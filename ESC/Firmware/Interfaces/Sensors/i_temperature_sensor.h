#ifndef I_TEMPERATURE_SENSOR_H
#define I_TEMPERATURE_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @file i_temperature_sensor.h
 * @brief Abstract interface for temperature sensors.
 *
 * Provides a hardware-agnostic API to read temperatures of the, MCU, PCB, ESC, motor,
 * or any critical component for thermal protection.
 */

/**
 * @brief Temperature sensor identifiers.
 */
typedef enum {
    TEMP_MCU,
    TEMP_PCB,
    TEMP_ESC,       /**< Temperature of ESC electronics */
    TEMP_MOTOR      /**< Temperature of motor winding or case */
} temperature_sensor_id_t;

/**
 * @brief Temperature sensor interface.
 */
typedef struct
{
    /**
     * @brief Initialize temperature sensors.
     * @return true if successful, false otherwise
     */
    bool (*init)(void);

    /**
     * @brief Read temperature of a specific sensor.
     * @param id Temperature sensor identifier
     * @param temp_value Pointer to store measured temperature value [°C]
     * @return true if successful, false otherwise
     */
    bool (*read)(temperature_sensor_id_t id, float* temp_value);

    /**
     * @brief Periodic non-blocking update function.
     * Should trigger new acquisition or refresh cached value.
     */
    void (*update)(void);

    /**
     * @brief Optional reset / calibration function
     */
    void (*calibrate)(void);

} i_temperature_sensor_t;

extern i_temperature_sensor_t* ITemperatureSensor;

#ifdef __cplusplus
}
#endif

#endif /* I_TEMPERATURE_SENSOR_H */
