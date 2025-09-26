/**
 * @file temperature_sensor_manager.c
 * @brief Manager that aggregates all temperature sensors and exposes unified interface.
 */

#include "../sensors_callbacks.h"
#include <stddef.h>
#include <string.h>

/* === Forward declarations of sensor drivers === */
extern i_temperature_sensor_t MCU_TemperatureSensor;
// extern i_temperature_sensor_t PCB_TemperatureSensor;
// extern i_temperature_sensor_t ESC_TemperatureSensor;
// extern i_temperature_sensor_t Motor_TemperatureSensor;

/* === Internal table of all sensor drivers === */
static i_temperature_sensor_t* sensor_drivers[] = {
    [TEMP_MCU]   = &MCU_TemperatureSensor,
    // [TEMP_PCB]   = &PCB_TemperatureSensor,
    // [TEMP_ESC]   = &ESC_TemperatureSensor,
    // [TEMP_MOTOR] = &Motor_TemperatureSensor
};

#define SENSOR_COUNT (sizeof(sensor_drivers)/sizeof(sensor_drivers[0]))

/* === Cache of last known temperature values === */
static float sensor_cache[SENSOR_COUNT] = {0.0f};
static bool  sensor_valid[SENSOR_COUNT] = {false};

/* === Local helpers === */
static bool Manager_Init(void);
static bool Manager_Read(temperature_sensor_id_t id, float* temp_value);
static void Manager_Update(void);
static void Manager_Calibrate(void);

/* === Global interface instance exposed to upper layers === */
static i_temperature_sensor_t manager_interface = {
    .init      = Manager_Init,
    .read      = Manager_Read,
    .update    = Manager_Update,
    .calibrate = Manager_Calibrate
};

i_temperature_sensor_t* ITemperatureSensor = &manager_interface;

/* ========================================================= */
/* === Implementation of Manager functions                === */
/* ========================================================= */

/**
 * @brief Initialize all available sensors.
 */
static bool Manager_Init(void)
{
    bool status = true;

    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_drivers[i] && sensor_drivers[i]->init) {
            status &= sensor_drivers[i]->init();
        }
    }

    memset(sensor_valid, 0, sizeof(sensor_valid));

    if (!SensorsCallbacks_Init())
    {
        return false;
    }
    
    return status;
}

/**
 * @brief Read the last cached value of a sensor.
 */
static bool Manager_Read(temperature_sensor_id_t id, float* temp_value)
{
    if (id >= SENSOR_COUNT || temp_value == NULL) {
        return false;
    }

    if (!sensor_valid[id]) {
        return false; // No valid measurement yet
    }

    *temp_value = sensor_cache[id];
    return true;
}

/**
 * @brief Update all sensors (non-blocking acquisition).
 * Each driver decides how it performs acquisition (DMA, interrupt, etc.).
 */
static void Manager_Update(void)
{
    // Non-blocking: ISR-driven drivers will update cache automatically
    // Optional: trigger acquisition for polling drivers if needed
}

/**
 * @brief Calibrate all sensors (optional).
 */
static void Manager_Calibrate(void)
{
    for (size_t i = 0; i < SENSOR_COUNT; i++) {
        if (sensor_drivers[i] && sensor_drivers[i]->calibrate) {
            sensor_drivers[i]->calibrate();
        }
    }
}

/* ========================================================= */
/* === ISR / Callback helper for non-blocking updates    === */
/* ========================================================= */

/**
 * @brief Called by ISR or ADC callback to store a new sample in cache.
 * @param id Sensor identifier
 * @param value Measured temperature value [°C]
 */
void TemperatureSensorManager_OnNewSample(temperature_sensor_id_t id, float value)
{
    if (id < SENSOR_COUNT) {
        sensor_cache[id] = value;
        sensor_valid[id] = true;
    }
}
