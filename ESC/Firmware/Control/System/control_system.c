#include "control.h"
#include "services.h"

/**
 * @brief Initialize the core system components.
 * 
 * This function initializes the system-level modules such as HAL and system clock.
 * It returns a control_status_t indicating whether initialization was successful.
 * 
 * @return CONTROL_OK if system initialization succeeded
 * @return CONTROL_ERROR if system initialization failed
 */
control_status_t System_Init(void){
    // Call the system service initialization function
    if (SSystem_Init() != SERVICE_OK)
    {
        // Return an error if system initialization fails
        return CONTROL_ERROR;
    }

    // Return OK if system initialization succeeds
    return CONTROL_OK;
}

/**
 * @brief Initialize the control layer of the system.
 * 
 * This function performs initialization of services, communication interfaces, 
 * and logging for the control module.
 * 
 * @return CONTROL_OK if control initialization succeeded
 * @return CONTROL_ERROR if any initialization step failed
 */
control_status_t Control_Init(void) {
    
    // Initialize all necessary services (drivers, middleware, etc.)
    if (Services_Init() != SERVICE_OK)
    {
        // Return an error if services fail to initialize
        return CONTROL_ERROR;
    }

    // Initialize the frame handler for receiving data frames (RX callback)
    DBFrameHandler_init();

    // Set the logging level for the PC terminal (e.g., debug messages)
    PCTerminal_SetLevel(LOG_LEVEL_DEBUG);

    // Return OK if all initialization steps succeed
    return CONTROL_OK;
}
