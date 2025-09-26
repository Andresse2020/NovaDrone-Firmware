#ifndef CONTROL_H
#define CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    CONTROL_OK = 0,       // Operation successful
    CONTROL_ERROR,       // General error 
} control_status_t;


/**
 * @brief Initialize system core (HAL, clock)
 *
 * This function should be called once at system startup.
 * It initializes HAL and system clock.
 *
 * @return true if all initializations succeed, false otherwise
 */
control_status_t System_Init(void);

/**
 * @brief Initialize all control modules of the application.
 *
 * This function should be called during system startup to configure and initialize
 * the various control modules required for system operation.
 */
control_status_t Control_Init(void);


void control_start(void);
void frame_handler_test(void);
void App_Init(void);
void command_handler_debug_process(void);

#ifdef __cplusplus
}
#endif

#endif // CONTROL_H