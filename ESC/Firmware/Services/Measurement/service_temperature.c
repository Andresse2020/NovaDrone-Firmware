#include "services.h"
#include "i_temperature_sensor.h"


float Service_GetMCU_Temp(void){
    float temp_local = 0.0;

    if (!ITemperatureSensor->read(TEMP_MCU, &temp_local)) {
        return 0.0f; // or NAN
    }

    return temp_local;
}
