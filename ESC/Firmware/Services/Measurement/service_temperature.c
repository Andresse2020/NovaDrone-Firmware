#include "services.h"
#include "i_temperature_sensor.h"


/**
 * @brief Get the latest measured MCU temperature
 * @return Temperature in °C, or 0.0f (or NAN) if the read operation failed
 *
 * @details This function queries the temperature sensor interface (ITemperatureSensor)
 *          to obtain the most recent value of the internal MCU temperature sensor.
 */
float Service_GetMCU_Temp(void) {
    float temp_local = 0.0;

    // Attempt to read the MCU temperature from the sensor manager
    if (!ITemperatureSensor->read(TEMP_MCU, &temp_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return temp_local;
}


/**
 * @brief Get the latest measured PCB temperature
 * @return Temperature in °C, or 0.0f (or NAN) if the read operation failed
 *
 * @details This function queries the temperature sensor interface (ITemperatureSensor)
 *          to obtain the most recent value of the external PCB temperature sensor.
 */
float Service_GetPCB_Temp(void) {
    float temp_local = 0.0;

    // Attempt to read the PCB temperature from the sensor manager
    if (!ITemperatureSensor->read(TEMP_PCB, &temp_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return temp_local;
}
