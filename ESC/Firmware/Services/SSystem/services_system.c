#include "service_generic.h"
#include "i_system.h"
#include "i_comm.h"
#include "i_temperature_sensor.h"
#include "i_inverter.h"
#include "i_time.h"
#include "i_time_oneshot.h"


/**
 * @brief Initialize the system core components.
 * 
 * This function initializes the fundamental system elements such as the HAL, 
 * system clock, and other core-level dependencies required before higher-level 
 * services can run.
 * 
 * @return SERVICE_OK    Core system initialized successfully
 * @return SERVICE_ERROR Initialization failed
 */
service_status_t SSystem_Init(void) {
    // Initialize the system core (HAL, clock, low-level dependencies)
    if (DSystem_Init() != I_OK)
    {
        // Return error if system core initialization fails
        return SERVICE_ERROR;
    }

    // Return success if initialization is completed
    return SERVICE_OK;
}


/**
 * @brief Initialize all required system services.
 * 
 * This function initializes essential drivers, communication interfaces, 
 * and sensors required for the system to operate. 
 * If any critical service fails, the function will immediately return 
 * an error status.
 * 
 * @return SERVICE_OK    All services initialized successfully
 * @return SERVICE_ERROR One or more services failed to initialize
 */
service_status_t Services_Init(void) {

    // Initialize low-level drivers (GPIO, ADC, timers, etc.)
    if (Driver_Init() != I_OK)
    {
        // Return error if drivers fail to initialize
        return SERVICE_ERROR;
    }

    // Initialize the debug communication interface
    if (!IComm_Debug->init())
    {
        // Return error if debug communication cannot be initialized
        return SERVICE_ERROR;
    }

    // Initialize the main/release communication interface
    if (!IComm_Release->init())
    {
        // Return error if release communication cannot be initialized
        return SERVICE_ERROR;
    }

    // Initialize the temperature sensor service
    if (!ITemperatureSensor->init())
    {
        return SERVICE_ERROR;
    }

    // Initialize the inverter driver
    if (!IInverter->init())
    {
        return SERVICE_ERROR;
    }

    // Optionally arm and enable the inverter if required at startup
    if (!IInverter->arm())
    {
        return SERVICE_ERROR;
    }

    // Enable the inverter outputs
    if (!IInverter->enable())
    {
        return SERVICE_ERROR;
    }

    // Initialize the time service
    if (!ITime->init())
    {
        
        return SERVICE_ERROR;
    }
    
    // Initialize the one-shot timer service
    if (!IOneShotTimer->init())
    {
        return SERVICE_ERROR;
    }

    // TODO: Add initialization of other services if required
    // Example: IVoltageSensor->init(), ICurrentSensor->init(), etc.

    // Return success if all services are properly initialized
    return SERVICE_OK;
}


/**
 * @brief Wrapper function to reset the MCU.
 *
 * Calls the existing System_Reset() function to trigger
 * a full MCU reset. Execution does not continue beyond this point.
 *
 * Notes:
 * - All volatile data and peripheral registers are reset.
 * - Make sure to save any critical information before calling this function.
 */
void Service_SystemReset(void)
{
    // Call the internal system reset function
    DSystem_Reset();
}
