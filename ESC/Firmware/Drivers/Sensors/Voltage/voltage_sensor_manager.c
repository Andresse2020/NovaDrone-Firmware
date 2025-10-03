/**
 * @file voltage_sensor_manager.c
 * @brief Manager that aggregates all voltage sensors and exposes unified interface.
 */

#include "../sensors_callbacks.h"
#include <stddef.h>
#include <string.h>

/* === Forward declarations of sensor drivers === */
extern i_voltage_sensor_t VoltageBusSensor;
// extern i_voltage_sensor_t Voltage3V3Sensor;
// extern i_voltage_sensor_t Voltage12VSensor;

/* === Internal table of all sensor drivers === */
static i_voltage_sensor_t* voltage_drivers[] = {
    [VOLTAGE_BUS] = &VoltageBusSensor,
    // [VOLTAGE_3V3] = &Voltage3V3Sensor,
    // [VOLTAGE_12V] = &Voltage12VSensor
};

#define VOLTAGE_SENSOR_COUNT (sizeof(voltage_drivers)/sizeof(voltage_drivers[0]))

/* === Cache of last known voltage values === */
static float voltage_cache[VOLTAGE_SENSOR_COUNT] = {0.0f};
static bool  voltage_valid[VOLTAGE_SENSOR_COUNT] = {false};

/* === Local helpers === */
static bool Manager_Init(void);
static bool Manager_Read(voltage_sensor_id_t id, float* voltage_value);
static void Manager_Reset(void);

/* === Global interface instance exposed to upper layers === */
static i_voltage_sensor_t manager_interface = {
    .init  = Manager_Init,
    .read  = Manager_Read,
    .reset = Manager_Reset
};

i_voltage_sensor_t* IVoltageSensor = &manager_interface;

/* ========================================================= */
/* === Implementation of Manager functions                === */
/* ========================================================= */

/**
 * @brief Initialize all available voltage sensors.
 */
static bool Manager_Init(void)
{
    bool status = true;

    for (size_t i = 0; i < VOLTAGE_SENSOR_COUNT; i++) {
        if (voltage_drivers[i] && voltage_drivers[i]->init) {
            status &= voltage_drivers[i]->init();
        }
    }

    memset(voltage_valid, 0, sizeof(voltage_valid));

    if (!SensorsCallbacks_Init())
    {
        return false;
    }

    return status;
}

/**
 * @brief Read the last cached value of a voltage sensor.
 */
static bool Manager_Read(voltage_sensor_id_t id, float* voltage_value)
{
    if (id >= VOLTAGE_SENSOR_COUNT || voltage_value == NULL) {
        return false;
    }

    if (!voltage_valid[id]) {
        return false; // No valid measurement yet
    }

    *voltage_value = voltage_cache[id];
    return true;
}

/**
 * @brief Reset or recalibrate all voltage sensors.
 */
static void Manager_Reset(void)
{
    for (size_t i = 0; i < VOLTAGE_SENSOR_COUNT; i++) {
        if (voltage_drivers[i] && voltage_drivers[i]->reset) {
            voltage_drivers[i]->reset();
        }
    }
}

/* ========================================================= */
/* === ISR / Callback helper for non-blocking updates    === */
/* ========================================================= */

/**
 * @brief Called by ISR or ADC callback to store a new voltage sample in cache.
 * @param id Voltage sensor identifier
 * @param value Measured voltage value [V]
 */
void VoltageSensorManager_OnNewSample(voltage_sensor_id_t id, float value)
{
    if (id < VOLTAGE_SENSOR_COUNT) {
        voltage_cache[id] = value;
        voltage_valid[id] = true;
    }
}
