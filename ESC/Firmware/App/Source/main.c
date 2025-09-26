#include <stdint.h>
#include "control.h"


/* -------------------------------------------------------------------------- */
/* Main loop ---------------------------------------------------------------- */
int main(void)
{
    // Initialize core system components (HAL, clock, etc.)
    System_Init();
    
    // Initialize the control layer, including services, communication, and logging
    Control_Init();

    // Infinite main loop
    while (1)
    {
        // Process any debug commands received via the command handler
        command_handler_debug_process();
        
        // Execute the main control logic of the system
        control_start();
    }
}
