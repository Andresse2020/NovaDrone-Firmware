/**
 * @file voltage_sensor_manager.c
 * @brief Manager for voltage sensors (3.3V, 12V, Vbus) using DMA and selective conversion.
 *
 * - ADC ISR sets a ready flag; no calculation in ISR.
 * - Update() processes only mapped channels when the corresponding flag is set.
 * - Mapping order in the table does not matter.
 *
 * Note:
 *  - adc?_buffer[] must be provided by the BSP (DMA buffers).
 *  - ADCx channel index enums (V3v3_SENS_VALUE, VBUS_SENS_VALUE, V12_SENS_VALUE)
 *    must be defined elsewhere (adc.h).
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
 * === Generic conversion helper
 * ========================================================= */
static inline float ConvertVoltage(uint16_t raw, float vref, float divider_ratio)
{
    return ((float)raw / 4095.0f) * vref * divider_ratio;
}

/* =========================================================
 * === Conversion function prototypes
 * ========================================================= */
static float Convert_VBus(uint16_t raw);
static float Convert_3V3(uint16_t raw);
static float Convert_12V(uint16_t raw);

/* =========================================================
 * === Mapping entry (order independent)
 * ========================================================= */
typedef struct {
    voltage_sensor_id_t id;       /* logical sensor id */
    uint8_t             adc_index;    /* 1 = ADC1, 2 = ADC2, 3 = ADC3 */
    uint8_t             channel_index;/* channel index inside the ADC's DMA buffer */
    float (*convert)(uint16_t raw);   /* conversion routine */
} sensor_map_entry_t;

/* =========================================================
 * === Sensor map (entries can be in any order)
 * === Map only the sensors you actually need
 * ========================================================= */
static const sensor_map_entry_t sensor_map[] = {
    { .id = VOLTAGE_3V3, .adc_index = 1, .channel_index = V3v3_SENS_VALUE, .convert = Convert_3V3 },
    { .id = VOLTAGE_BUS, .adc_index = 2, .channel_index = VBUS_SENS_VALUE, .convert = Convert_VBus },
    { .id = VOLTAGE_12V, .adc_index = 3, .channel_index = V12_SENS_VALUE,  .convert = Convert_12V },
};

#define SENSOR_MAP_COUNT (sizeof(sensor_map) / sizeof(sensor_map[0]))

/* =========================================================
 * === Local cache and validity (indexed by voltage_sensor_id_t)
 * ========================================================= */
static float sensor_cache[VOLT_SENSOR_COUNT];
static bool  sensor_valid[VOLT_SENSOR_COUNT];

/* =========================================================
 * === Internal helper: process given ADC buffer (order independent)
 * ========================================================= */
static void ProcessAdcBuffer(uint8_t adc_index, volatile uint16_t* buffer, volatile bool* flag)
{
    if (!(*flag))
        return;

    *flag = false;

    for (uint32_t m = 0; m < SENSOR_MAP_COUNT; ++m) {
        const sensor_map_entry_t* sensor_x = &sensor_map[m];

        if (sensor_x->adc_index != adc_index || sensor_x->convert == NULL)
            continue;

        /* Protect against bad channel index (simple defensive check) */
        /* Note: we assume buffer is large enough; BSP must guarantee ADCx_CHANNELS value */
        float v = sensor_x->convert(buffer[sensor_x->channel_index]);
        sensor_cache[sensor_x->id] = v;
        sensor_valid[sensor_x->id] = true;
    }
}

/* =========================================================
 * === Public API
 * ========================================================= */
bool VoltageSensorManager_Init(void)
{
    memset(sensor_cache, 0, sizeof(sensor_cache));
    memset(sensor_valid, 0, sizeof(sensor_valid));
    memset((void*)&adc_flags, 0, sizeof(adc_flags));

    /* Ensure sensors callbacks (ADC start) are initialized */
    if (!SensorsCallbacks_IsInitialized()) {
        if (!SensorsCallbacks_Init()) {
            return false;
        }
    }

    return true;
}

void VoltageSensorManager_Update(void)
{
    ProcessAdcBuffer(1, adc1_buffer, &adc_flags.adc1_ready);
    ProcessAdcBuffer(2, adc2_buffer, &adc_flags.adc2_ready);
    ProcessAdcBuffer(3, adc3_buffer, &adc_flags.adc3_ready);
}

bool VoltageSensorManager_Read(voltage_sensor_id_t id, float* out)
{
    if (!out || id >= VOLT_SENSOR_COUNT || !sensor_valid[id])
        return false;

    *out = sensor_cache[id];
    return true;
}

/* =========================================================
 * === ISR callbacks (only set flags)
 * === These should be called from your HAL ADC conv complete callbacks
 * ========================================================= */
void VoltageSensorManager_OnEndOfBlock_ADC1(void)
{
    adc_flags.adc1_ready = true;
}

void VoltageSensorManager_OnEndOfBlock_ADC2(void)
{
    adc_flags.adc2_ready = true;
}

void VoltageSensorManager_OnEndOfBlock_ADC3(void)
{
    adc_flags.adc3_ready = true;
}

/* =========================================================
 * === Conversion functions
 * ========================================================= */
static float Convert_VBus(uint16_t raw)
{
    /* Vbus divider example: ratio = 11.0 */
    return ConvertVoltage(raw, 3.3f, 11.0f);
}

static float Convert_3V3(uint16_t raw)
{
    /* 3.3V sense divider example: ratio = 2.0 */
    return ConvertVoltage(raw, 3.3f, 2.0f);
}

static float Convert_12V(uint16_t raw)
{
    /* 12V divider example: ratio = 7.8 */
    return ConvertVoltage(raw, 3.3f, 7.8f);
}

/* =========================================================
 * === Public interface instance (exposed to upper layers)
 * ========================================================= */
static void VoltageSensorManager_Reset(void)
{
    memset(sensor_valid, 0, sizeof(sensor_valid));
}

static i_voltage_sensor_t manager_interface = {
    .init      = VoltageSensorManager_Init,
    .update    = VoltageSensorManager_Update,
    .read      = VoltageSensorManager_Read,
    .reset     = VoltageSensorManager_Reset
};

i_voltage_sensor_t* IVoltageSensor = &manager_interface;
