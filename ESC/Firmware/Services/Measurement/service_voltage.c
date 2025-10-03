#include "services.h"
#include "i_voltage_sensor.h"


float Service_GetBus_Voltage(void) {
    float voltage_local = 0.0;

    // Attempt to read the BUS Voltage from the sensor manager
    if (!IVoltageSensor->read(VOLTAGE_BUS, &voltage_local)) {
        return 0.0f; // Return fallback value if read failed
    }

    return voltage_local;
}