/**
 * @file temperature_sensor_manager.c
 * @brief Manager for temperature sensors (MCU + PCB) using DMA and selective conversion.
 *
 * - The ADC ISR only sets ready flags — no computation is performed in interrupt context.
 * - The Update() function processes mapped ADC channels only when their corresponding ready flag is set.
 * - The mapping order in the table does NOT matter; it is dynamically resolved.
 *
 * Notes:
 *  - adc?_buffer[] must be defined in the BSP / ADC driver (used as DMA destination).
 *  - ADCx channel index macros (e.g., MCU_SENS_VALUE, PCB_SENS_VALUE) must be defined elsewhere.
 *  - TEMP_SENSOR_COUNT defines the total number of logical sensors handled (MCU + PCB).
 */

#include "../sensors_callbacks.h"
#include <string.h>

/* =========================================================
 * === ADC End-of-block flags (set only in ISR)
 * ========================================================= */
/**
 * @brief Flags indicating when each ADC has completed a DMA block transfer.
 * 
 * These flags are set from the corresponding ISR callbacks and cleared
 * after the DMA buffer has been processed by the Update() function.
 */
typedef struct {
    volatile bool adc1_ready; /**< True if ADC1 has new data */
    volatile bool adc2_ready; /**< True if ADC2 has new data */
    volatile bool adc3_ready; /**< True if ADC3 has new data */
} adc_flag_t;

static adc_flag_t adc_flags = {0};

/* =========================================================
 * === Conversion function prototypes
 * ========================================================= */
/**
 * @brief Convert raw ADC data to MCU temperature (internal sensor).
 */
static float Convert_TempMCU(uint16_t raw);

/**
 * @brief Convert raw ADC data to PCB temperature (external sensor).
 */
static float Convert_TempPCB(uint16_t raw);

/* =========================================================
 * === Sensor Mapping (order independent)
 * ========================================================= */
/**
 * @brief Defines how each temperature sensor is linked to a physical ADC channel.
 */
typedef struct {
    temperature_sensor_id_t id;        /**< Logical sensor ID */
    uint8_t adc_index;                 /**< ADC instance number (1, 2, or 3) */
    uint8_t channel_index;             /**< Channel index within the DMA buffer */
    float (*convert)(uint16_t raw);    /**< Pointer to conversion routine */
} sensor_map_entry_t;

/**
 * @brief Table describing which ADC and channel correspond to each logical temperature sensor.
 * @note The table order does NOT matter; each entry is matched dynamically by adc_index.
 */
static const sensor_map_entry_t sensor_map[] = {
    { .id = TEMP_MCU, .adc_index = 1, .channel_index = MCU_SENS_VALUE, .convert = Convert_TempMCU },
    { .id = TEMP_PCB, .adc_index = 2, .channel_index = PCB_SENS_VALUE, .convert = Convert_TempPCB },
};

#define SENSOR_MAP_COUNT (sizeof(sensor_map) / sizeof(sensor_map[0]))

/* =========================================================
 * === Local cache and validity tracking
 * ========================================================= */
/**
 * @brief Stores the latest temperature readings and validity state for each sensor.
 */
static float sensor_cache[TEMP_SENSOR_COUNT] = {0.0f}; /**< Last valid temperature values */
static bool  sensor_valid[TEMP_SENSOR_COUNT] = {false}; /**< Validity flags per sensor */

/* =========================================================
 * === Internal helper: process ADC buffers (order independent)
 * ========================================================= */
/**
 * @brief Process all temperature sensors associated with a given ADC buffer.
 *
 * @param adc_index ADC instance number (1, 2, or 3)
 * @param buffer Pointer to ADC DMA buffer containing raw samples
 * @param flag Pointer to the ADC ready flag (set by ISR)
 *
 * This function:
 *  - Checks if the given ADC flag is set.
 *  - Converts raw ADC values to physical temperature using the mapped conversion function.
 *  - Updates the cache and sets the corresponding validity flag.
 */
static void ProcessAdcBuffer(uint8_t adc_index, volatile uint16_t* buffer, volatile bool* flag)
{
    if (!(*flag))
        return;

    *flag = false; // Clear flag to acknowledge data processed

    for (uint32_t i = 0; i < SENSOR_MAP_COUNT; ++i) {
        const sensor_map_entry_t* sensor_x = &sensor_map[i];

        if (sensor_x->adc_index != adc_index || sensor_x->convert == NULL)
            continue;

        float temperature = sensor_x->convert(buffer[sensor_x->channel_index]);
        sensor_cache[sensor_x->id] = temperature;
        sensor_valid[sensor_x->id] = true;
    }
}

/* =========================================================
 * === Public API
 * ========================================================= */

/**
 * @brief Initialize the Temperature Sensor Manager.
 *
 * This function:
 *  - Clears cached data and flags.
 *  - Ensures the shared ADC DMA callback subsystem is initialized.
 *
 * @retval true  Initialization successful.
 * @retval false Initialization failed.
 */
bool TemperatureSensorManager_Init(void)
{
    memset(sensor_cache, 0, sizeof(sensor_cache));
    memset(sensor_valid, 0, sizeof(sensor_valid));
    memset((void*)&adc_flags, 0, sizeof(adc_flags));

    /* Ensure ADC DMA and callbacks are properly set up */
    if (!SensorsCallbacks_IsInitialized()) {
        if (!SensorsCallbacks_Init()) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Update cached temperature readings from DMA buffers.
 *
 * Should be called periodically from the main loop or scheduler.
 * Processes ADC buffers that have been marked as "ready" by ISR callbacks.
 */
void TemperatureSensorManager_Update(void)
{
    ProcessAdcBuffer(1, adc1_buffer, &adc_flags.adc1_ready);
    ProcessAdcBuffer(2, adc2_buffer, &adc_flags.adc2_ready);
    ProcessAdcBuffer(3, adc3_buffer, &adc_flags.adc3_ready);
}

/**
 * @brief Retrieve the most recent valid temperature reading.
 *
 * @param id  Logical temperature sensor ID (TEMP_MCU or TEMP_PCB)
 * @param out Pointer to store the resulting temperature value (°C)
 *
 * @retval true  Reading available and copied to *out.
 * @retval false Invalid ID, null pointer, or no valid reading yet.
 */
bool TemperatureSensorManager_Read(temperature_sensor_id_t id, float* out)
{
    if (!out || id >= TEMP_SENSOR_COUNT || !sensor_valid[id])
        return false;

    *out = sensor_cache[id];
    return true;
}

/* =========================================================
 * === ISR Callbacks (called from ADC conversion complete ISR)
 * ========================================================= */

/**
 * @brief ISR callback for ADC1 end-of-block event.
 */
void TemperatureSensorManager_OnEndOfBlock_ADC1(void)
{
    adc_flags.adc1_ready = true;
}

/**
 * @brief ISR callback for ADC2 end-of-block event.
 */
void TemperatureSensorManager_OnEndOfBlock_ADC2(void)
{
    adc_flags.adc2_ready = true;
}

/**
 * @brief ISR callback for ADC3 end-of-block event.
 */
void TemperatureSensorManager_OnEndOfBlock_ADC3(void)
{
    adc_flags.adc3_ready = true;
}

/* =========================================================
 * === Conversion functions
 * ========================================================= */

/**
 * @brief Convert raw ADC data to MCU internal temperature (°C).
 *
 * Uses STM32 HAL's built-in temperature calculation macro, based on
 * the internal temperature sensor calibration values.
 *
 * @param raw Raw ADC value (12-bit)
 * @return Temperature in degrees Celsius.
 */
static float Convert_TempMCU(uint16_t raw)
{
    const float vref = 3.3f;  /* Reference voltage in volts */
    return __HAL_ADC_CALC_TEMPERATURE(vref * 1000.0f, raw, ADC_RESOLUTION_12B);
}

/**
 * @brief Convert raw ADC data to PCB temperature (°C).
 *
 * Based on an external analog sensor with a linear response:
 *  - 1.90 V at 0°C
 *  - 2.89 V at 80°C
 *
 * @param raw Raw ADC value (12-bit)
 * @return Temperature in degrees Celsius.
 */
static float Convert_TempPCB(uint16_t raw)
{
    const float vref = 3.3f;
    const float V0   = 1.90f; /* Output voltage at 0°C */
    const float V80  = 2.89f; /* Output voltage at 80°C */
    const float T0   = 0.0f;
    const float T80  = 80.0f;

    float voltage = ((float)raw / 4095.0f) * vref;
    return ((voltage - V0) * (T80 - T0)) / (V80 - V0) + T0;
}

/* =========================================================
 * === Public interface instance (used by upper layers)
 * ========================================================= */

/**
 * @brief Public interface exposing temperature sensor manager functions.
 */
static i_temperature_sensor_t temp_manager_interface = {
    .init      = TemperatureSensorManager_Init,
    .update    = TemperatureSensorManager_Update,
    .read      = TemperatureSensorManager_Read,
    .calibrate = NULL, /**< Optional future extension */
};

/**
 * @brief Global interface pointer used by service or application layers.
 */
i_temperature_sensor_t* ITemperatureSensor = &temp_manager_interface;
