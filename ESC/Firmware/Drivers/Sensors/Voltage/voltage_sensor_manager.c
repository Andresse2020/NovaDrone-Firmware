/**
 * @file voltage_sensor_manager.c
 * @brief Voltage sensor manager (3.3V, 12V, VBUS) with DMA acquisition and deferred processing.
 *
 * This module provides a non-blocking management layer for voltage sensors.
 * ADC conversions are handled by DMA; the ISR only sets a ready flag,
 * while the processing and conversion to real voltages are done later in the main loop.
 *
 * Features:
 *  - Fully decoupled ADC interrupt and data processing.
 *  - Order-independent mapping of logical sensors to ADC instances.
 *  - Centralized cache for latest converted voltages.
 *
 * Requirements:
 *  - adc?_buffer[] DMA buffers must be provided by the BSP layer.
 *  - ADCx channel index enums (e.g., V3v3_SENS_VALUE, VBUS_SENS_VALUE) must be defined in adc.h.
 */

#include "../sensors_callbacks.h"
#include <string.h>

/* =========================================================
 * === ADC End-of-block flags (set by ISR only)
 * =========================================================
 * Each flag indicates that the corresponding ADC buffer has
 * completed a DMA conversion sequence and is ready to be processed.
 */
typedef struct {
    volatile bool adc1_ready;
    volatile bool adc2_ready;
    volatile bool adc3_ready;
} adc_flag_t;

static adc_flag_t adc_flags = {0};


/* =========================================================
 * === Generic voltage conversion helper
 * =========================================================
 * Converts a raw ADC sample into a real voltage value using:
 *      V = (raw / 4095) * Vref * divider_ratio
 */
static inline float ConvertVoltage(uint16_t raw, float vref, float divider_ratio)
{
    return ((float)raw / 4095.0f) * vref * divider_ratio;
}


/* =========================================================
 * === Conversion functions (sensor-specific)
 * ========================================================= */
static float Convert_VBus(uint16_t raw);
static float Convert_3V3(uint16_t raw);
static float Convert_12V(uint16_t raw);


/* =========================================================
 * === Mapping definition (ADC <-> logical sensor)
 * =========================================================
 * Each entry defines which ADC buffer and channel correspond
 * to a logical voltage sensor, and which conversion function to use.
 * The table order does not matter.
 */
typedef struct {
    voltage_sensor_id_t id;        /**< Logical sensor ID */
    uint8_t             adc_index; /**< ADC instance index: 1, 2, or 3 */
    uint8_t             channel_index; /**< Index inside ADC DMA buffer */
    float (*convert)(uint16_t raw);    /**< Pointer to conversion function */
} sensor_map_entry_t;

/* Actual mapping configuration */
static const sensor_map_entry_t sensor_map[] = {
    { .id = VOLTAGE_3V3, .adc_index = 1, .channel_index = V3v3_SENS_VALUE, .convert = Convert_3V3 },
    { .id = VOLTAGE_BUS, .adc_index = 2, .channel_index = VBUS_SENS_VALUE, .convert = Convert_VBus },
    { .id = VOLTAGE_12V, .adc_index = 3, .channel_index = V12_SENS_VALUE,  .convert = Convert_12V },
};

#define SENSOR_MAP_COUNT (sizeof(sensor_map) / sizeof(sensor_map[0]))


/* =========================================================
 * === Local cache for converted voltages
 * =========================================================
 * Stores the latest valid readings for each voltage sensor.
 * `sensor_valid[]` indicates whether the cached value is fresh.
 */
static float sensor_cache[VOLT_SENSOR_COUNT];
static bool  sensor_valid[VOLT_SENSOR_COUNT];


/* =========================================================
 * === Internal helper: process ADC buffer when ready
 * =========================================================
 * This function is called by VoltageSensorManager_Update() when
 * an ADC ready flag is set. It updates the cache for all sensors
 * associated with the given ADC instance.
 */
static void ProcessAdcBuffer(uint8_t adc_index, volatile uint16_t* buffer, volatile bool* flag)
{
    if (!(*flag))
        return;

    *flag = false; // Clear flag immediately to avoid race conditions

    for (uint32_t m = 0; m < SENSOR_MAP_COUNT; ++m) {
        const sensor_map_entry_t* sensor_x = &sensor_map[m];

        if (sensor_x->adc_index != adc_index || sensor_x->convert == NULL)
            continue;

        // Defensive: ensure index validity (buffer size guaranteed by BSP)
        float v = sensor_x->convert(buffer[sensor_x->channel_index]);
        sensor_cache[sensor_x->id] = v;
        sensor_valid[sensor_x->id] = true;
    }
}


/* =========================================================
 * === Public API
 * ========================================================= */

/**
 * @brief Initialize the voltage sensor manager
 * @return true on success, false on initialization error
 */
bool VoltageSensorManager_Init(void)
{
    memset(sensor_cache, 0, sizeof(sensor_cache));
    memset(sensor_valid, 0, sizeof(sensor_valid));
    memset((void*)&adc_flags, 0, sizeof(adc_flags));

    // Ensure that the common sensor callback layer is ready
    if (!SensorsCallbacks_IsInitialized()) {
        if (!SensorsCallbacks_Init()) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Process ready ADC buffers and update voltage cache
 *
 * @details Should be called periodically (e.g., in main loop or scheduler).
 *          Converts raw ADC data into voltages for all sensors
 *          whose ADC has signaled completion.
 */
void VoltageSensorManager_Update(void)
{
    ProcessAdcBuffer(1, adc1_buffer, &adc_flags.adc1_ready);
    ProcessAdcBuffer(2, adc2_buffer, &adc_flags.adc2_ready);
    ProcessAdcBuffer(3, adc3_buffer, &adc_flags.adc3_ready);
}

/**
 * @brief Read the latest voltage value for a given sensor
 * @param id  Sensor ID (see voltage_sensor_id_t)
 * @param out Pointer to output variable
 * @return true if valid data available, false otherwise
 */
bool VoltageSensorManager_Read(voltage_sensor_id_t id, float* out)
{
    if (!out || id >= VOLT_SENSOR_COUNT || !sensor_valid[id])
        return false;

    *out = sensor_cache[id];
    return true;
}

/**
 * @brief Reset all internal states of the Voltage Sensor Manager.
 *
 * This function clears:
 *  - Cached voltage values (sensor_cache)
 *  - Validity flags (sensor_valid)
 *  - ADC end-of-block flags (adc_flags)
 *
 * It is typically used when restarting the measurement system,
 * changing operating modes, or recovering from an error condition.
 * After this call, all voltage readings are considered invalid
 * until the next valid ADC update occurs.
 */
static void VoltageSensorManager_Reset(void)
{
    memset(sensor_cache, 0, sizeof(sensor_cache));   // Clear cached voltage values
    memset(sensor_valid, 0, sizeof(sensor_valid));   // Invalidate all readings
    memset((void*)&adc_flags, 0, sizeof(adc_flags)); // Reset ADC ready flags
}


/* =========================================================
 * === ISR callbacks (called from HAL ADC handlers)
 * =========================================================
 * These functions should be called from HAL_ADC_ConvCpltCallback()
 * or similar ISR context to signal that new DMA data is available.
 */
void VoltageSensorManager_OnEndOfBlock_ADC1(void) { adc_flags.adc1_ready = true; }
void VoltageSensorManager_OnEndOfBlock_ADC2(void) { adc_flags.adc2_ready = true; }
void VoltageSensorManager_OnEndOfBlock_ADC3(void) { adc_flags.adc3_ready = true; }


/* =========================================================
 * === Conversion functions (ADC raw → voltage)
 * ========================================================= */
static float Convert_VBus(uint16_t raw) { return ConvertVoltage(raw, 3.3f, 11.0f); }  // Example: divider 11:1
static float Convert_3V3(uint16_t raw)  { return ConvertVoltage(raw, 3.3f,  2.0f); }  // Example: divider 2:1
static float Convert_12V(uint16_t raw)  { return ConvertVoltage(raw, 3.3f,  7.8f); }  // Example: divider 7.8:1


/* =========================================================
 * === Interface export (for dependency injection)
 * ========================================================= */

/**
 * @brief Public instance of voltage sensor interface
 *
 * This instance is exposed globally and can be accessed via
 * the pointer `IVoltageSensor`, providing a uniform interface
 * for all voltage sensor operations.
 */
static i_voltage_sensor_t manager_interface = {
    .init   = VoltageSensorManager_Init,
    .update = VoltageSensorManager_Update,
    .read   = VoltageSensorManager_Read,
    .reset  = VoltageSensorManager_Reset
};

i_voltage_sensor_t* IVoltageSensor = &manager_interface;