#include "services.h"
#include "i_voltage_sensor.h"


/**
 * @brief Get the latest measured bus voltage
 * @return Bus voltage in volts, or 0.0f if the read operation failed
 *
 * @details Queries the voltage sensor interface to retrieve the most recent
 *          bus voltage measurement. Returns a fallback value if unavailable.
 */
float Service_GetBus_Voltage(void) {
    float voltage_local = 0.0;

    // Attempt to read the bus voltage from the voltage sensor manager
    if (!IVoltageSensor->read(VOLTAGE_BUS, &voltage_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return voltage_local;
}


/**
 * @brief Get the latest measured 3.3V rail voltage
 * @return 3.3V rail voltage in volts, or 0.0f if the read operation failed
 *
 * @details Queries the voltage sensor interface to retrieve the most recent
 *          3.3V power rail measurement. Returns a fallback value if unavailable.
 */
float Service_Get3v3_Voltage(void) {
    float voltage_local = 0.0;

    // Attempt to read the 3.3V rail voltage from the voltage sensor manager
    if (!IVoltageSensor->read(VOLTAGE_3V3, &voltage_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return voltage_local;
}


/**
 * @brief Get the latest measured 12V rail voltage
 * @return 12V rail voltage in volts, or 0.0f if the read operation failed
 *
 * @details Queries the voltage sensor interface to retrieve the most recent
 *          12V power rail measurement. Returns a fallback value if unavailable.
 */
float Service_Get12V_Voltage(void) {
    float voltage_local = 0.0;

    // Attempt to read the 12V rail voltage from the voltage sensor manager
    if (!IVoltageSensor->read(VOLTAGE_12V, &voltage_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return voltage_local;
}
