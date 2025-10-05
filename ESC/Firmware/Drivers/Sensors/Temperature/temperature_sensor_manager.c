/**
 * @file temperature_sensor_manager.c
 * @brief Manager for temperature sensors (MCU + PCB) using DMA and selective conversion.
 *
 * - ADC ISR only sets ready flags — no math in interrupt context.
 * - Update() processes mapped channels only when their ADC flag is set.
 * - The mapping order in the table does NOT matter.
 *
 * Notes:
 *  - adc?_buffer[] must be defined in the BSP / ADC driver (DMA buffers).
 *  - ADCx channel index macros (MCU_SENS_VALUE, PCB_SENS_VALUE) must be defined elsewhere.
 *  - TEMP_SENSOR_COUNT defines the number of logical sensors (MCU + PCB).
 */

#include "../sensors_callbacks.h"
#include <string.h>


/* =========================================================
 * === ADC End-of-block flags (set only in ISR)
 * ========================================================= */
typedef struct {
    volatile bool adc1_ready;
    volatile bool adc2_ready;
    volatile bool adc3_ready;
} adc_flag_t;

static adc_flag_t adc_flags = {0};

/* =========================================================
 * === Conversion function prototypes
 * ========================================================= */
static float Convert_TempMCU(uint16_t raw);
static float Convert_TempPCB(uint16_t raw);

/* =========================================================
 * === Sensor Mapping (order independent)
 * ========================================================= */
typedef struct {
    temperature_sensor_id_t id;   /* logical sensor id */
    uint8_t adc_index;            /* 1 = ADC1, 2 = ADC2, 3 = ADC3 */
    uint8_t channel_index;        /* Channel index in the DMA buffer */
    float (*convert)(uint16_t raw); /* Pointer to conversion function */
} sensor_map_entry_t;

/**
 * @brief Table describing which ADC and channel correspond to each sensor.
 * @note The order does NOT matter; each entry is matched dynamically.
 */
static const sensor_map_entry_t sensor_map[] = {
    { .id = TEMP_MCU, .adc_index = 1, .channel_index = MCU_SENS_VALUE, .convert = Convert_TempMCU },
    { .id = TEMP_PCB, .adc_index = 2, .channel_index = PCB_SENS_VALUE, .convert = Convert_TempPCB },
};

#define SENSOR_MAP_COUNT (sizeof(sensor_map) / sizeof(sensor_map[0]))

/* =========================================================
 * === Local cache and validity (indexed by temperature_sensor_id_t)
 * ========================================================= */
static float sensor_cache[TEMP_SENSOR_COUNT] = {0.0f};
static bool  sensor_valid[TEMP_SENSOR_COUNT] = {false};

/* =========================================================
 * === Internal helper: process ADC buffers (order independent)
 * ========================================================= */
/**
 * @brief Process all sensors linked to a specific ADC buffer.
 * @param adc_index ADC instance number (1, 2, or 3)
 * @param buffer Pointer to ADC DMA buffer
 * @param flag Pointer to the "ready" flag
 */
static void ProcessAdcBuffer(uint8_t adc_index, volatile uint16_t* buffer, volatile bool* flag)
{
    if (!(*flag))
        return;

    *flag = false;

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
bool TemperatureSensorManager_Init(void)
{
    memset(sensor_cache, 0, sizeof(sensor_cache));
    memset(sensor_valid, 0, sizeof(sensor_valid));
    memset((void*)&adc_flags, 0, sizeof(adc_flags));

    /* Ensure the ADC DMA and callback infrastructure is initialized */
    if (!SensorsCallbacks_IsInitialized()) {
        if (!SensorsCallbacks_Init()) {
            return false;
        }
    }

    return true;
}

void TemperatureSensorManager_Update(void)
{
    ProcessAdcBuffer(1, adc1_buffer, &adc_flags.adc1_ready);
    ProcessAdcBuffer(2, adc2_buffer, &adc_flags.adc2_ready);
    ProcessAdcBuffer(3, adc3_buffer, &adc_flags.adc3_ready);
}

bool TemperatureSensorManager_Read(temperature_sensor_id_t id, float* out)
{
    if (!out || id >= TEMP_SENSOR_COUNT || !sensor_valid[id])
        return false;

    *out = sensor_cache[id];
    return true;
}

/* =========================================================
 * === ISR Callbacks (to be called from ADC conversion complete interrupt)
 * ========================================================= */
void TemperatureSensorManager_OnEndOfBlock_ADC1(void)
{
    adc_flags.adc1_ready = true;
}

void TemperatureSensorManager_OnEndOfBlock_ADC2(void)
{
    adc_flags.adc2_ready = true;
}

void TemperatureSensorManager_OnEndOfBlock_ADC3(void)
{
    adc_flags.adc3_ready = true;
}

/* =========================================================
 * === Conversion functions
 * ========================================================= */
static float Convert_TempMCU(uint16_t raw)
{
    const float vref = 3.3f;  /* Reference voltage in volts */
    /* Use HAL macro for internal temperature sensor */
    return __HAL_ADC_CALC_TEMPERATURE(vref * 1000.0f, raw, ADC_RESOLUTION_12B);
}

static float Convert_TempPCB(uint16_t raw)
{
    const float vref = 3.3f;  /* Reference voltage */
    const float V0   = 1.90f; /* Output voltage at 0°C */
    const float V80  = 2.89f; /* Output voltage at 80°C */
    const float T0   = 0.0f;
    const float T80  = 80.0f;

    float voltage = ((float)raw / 4095.0f) * vref;
    return ((voltage - V0) * (T80 - T0)) / (V80 - V0) + T0;
}

/* =========================================================
 * === Public interface instance (exposed to upper layers)
 * ========================================================= */

static i_temperature_sensor_t temp_manager_interface = {
    .init      = TemperatureSensorManager_Init,
    .update    = TemperatureSensorManager_Update,
    .read      = TemperatureSensorManager_Read,
    .calibrate = NULL,
};

i_temperature_sensor_t* ITemperatureSensor = &temp_manager_interface;
