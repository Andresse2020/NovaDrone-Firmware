/**
 * @file command_handler_debug.c
 * @brief Debug command handler for system/control commands over DBProtocol.
 *
 * This module provides a structured workflow:
 *  1. Acquire frame from the DBFrameHandler
 *  2. Decode frame into structured protocol message
 *  3. Validate command support
 *  4. Dispatch system/control commands
 *
 * Notes:
 *  - The module assumes a single active frame handler (DBFrameHandler) and
 *    protocol implementation (DBProtocol).
 *  - Commands are dispatched using a switch-case on the command ID.
 *  - Extensible: additional command categories can be added with separate dispatch functions.
 */

#include "services.h"
#include "frame_handler.h"
#include "protocol.h"

/// Maximum frame buffer size
#define FRAME_MAX_SIZE 64

/// External frame handler and protocol interfaces
extern const i_frame_handler_t* DBFrameHandler;
extern const i_protocol_t* DBProtocol;

/// Internal frame buffer
static uint8_t frame[FRAME_MAX_SIZE];
static uint16_t frame_length = 0;

/// Structured protocol message
static protocol_msg_t msg;

/**
 * @brief Acquire a single debug frame from the frame handler.
 *
 * @return true if a frame was successfully acquired, false otherwise
 */
static bool acquire_debug_frame(void)
{
    if(DBFrameHandler == NULL)
        return false;

    // Check if a frame is available
    if(!DBFrameHandler->available())
        return false;

    // Pop the frame into local buffer
    if(!DBFrameHandler->pop(frame, &frame_length))
    {
        frame_length = 0;
        return false;
    }

    return true;
}

/**
 * @brief Decode the acquired frame into a structured protocol message
 *        and validate the command.
 *
 * @return true if the message is successfully decoded and supported, false otherwise
 */
static bool acquire_and_decode_debug_message(void)
{
    if(DBProtocol == NULL)
        return false;

    // Decode the frame into a structured protocol message
    protocol_status_t status = DBProtocol->decode(frame, frame_length, &msg);
    if(status != PROTOCOL_OK)
    {
        frame_length = 0;  // Reset frame length on failure
        LOG_ERROR("Invalid or corrupted command");
        return false;
    }

    // Validate that the command is supported
    if(!DBProtocol->is_supported(msg.command_id))
    {
        frame_length = 0;
        return false;
    }

    return true;
}

/**
 * @brief Dispatch system commands received via protocol messages.
 *
 * This function interprets the command ID from the received message
 * and executes the corresponding system action.
 *
 * @param msg Pointer to the received protocol message. If NULL, the function returns immediately.
 */
static void dispatch_system_command(const protocol_msg_t* msg)
{
    // Check for null pointer to avoid dereferencing invalid memory
    if(msg == NULL)
        return;

    // Switch based on the command ID in the protocol message
    switch(msg->command_id)
    {
        case CMD_HELP:
        {
            // Display the help information for available commands
            DBProtocol->show_help();
            break;
        }

        case CMD_VERSION:
        {
            // Display firmware version
            const char* fw_version = "FW v1.0.0";
            LOG_INFO("Firmware version: %s", fw_version);
            break;
        }

        case CMD_PING:
            // Respond to ping command with a simple "pong" message
            LOG_INFO("pong");
            break;

        case CMD_RESET:
        {
            // Trigger a system reset using the service wrapper
            Service_SystemReset();
            break;
        }

        case CMD_STATUS:
        {
            // Display system status including running time and system frequency

            char time_str[32];  // Buffer to hold formatted running time string
            Service_GetRunTimeString(time_str, sizeof(time_str));  // Convert system tick to readable time

            uint32_t freq_mhz = Service_GetSysFrequencyMHz();  // Get system clock frequency in MHz

            char mcu_temp[64];
            char pcb_temp[64];
            char bus_voltage[64];

            Service_FloatToString(Service_GetMCU_Temp(), mcu_temp, 2);
            Service_FloatToString(Service_GetPCB_Temp(), pcb_temp, 2);
            Service_FloatToString(Service_GetBus_Voltage(), bus_voltage, 2);
            
            // Log system status to the debug terminal
            LOG_INFO("System status:");
            LOG_INFO("System frequency: %lu MHz", freq_mhz);
            LOG_INFO("System running time: %s", time_str);
            LOG_INFO("System MCU Temperature: %s °C", mcu_temp);
            LOG_INFO("System PCB Temperature: %s °C", pcb_temp);
            LOG_INFO("System BUS Voltage: %s Volts", bus_voltage);
            break;
        }

        case CMD_CLEAR:
        {
            // Clear the terminal screen using ANSI escape codes
            LOG_INFO("\033[2J\033[H");
            break;
        }

        case CMD_LOGLEVEL:
        {
            // Check that the command has at least one argument
            // and that this argument is a string.
            if (msg->arg_count < 1 || msg->args[0].type != PROTOCOL_ARG_STRING) {
                LOG_NONE("Usage: loglevel <none|error|warn|info|debug|trace>");
                break;
            }

            // Get the argument value as a string
            const char* arg = msg->args[0].value.str;
            log_level_t level;

            // Compare the argument against the known log levels
            if      (strcmp(arg, "none")  == 0) level = LOG_LEVEL_NONE;
            else if (strcmp(arg, "error") == 0) level = LOG_LEVEL_ERROR;
            else if (strcmp(arg, "warn")  == 0) level = LOG_LEVEL_WARN;
            else if (strcmp(arg, "info")  == 0) level = LOG_LEVEL_INFO;
            else if (strcmp(arg, "debug") == 0) level = LOG_LEVEL_DEBUG;
            else if (strcmp(arg, "trace") == 0) level = LOG_LEVEL_TRACE;
            else {
                // If the argument doesn't match any valid level,
                // print an error and show the list of valid options
                LOG_NONE("Invalid log level: %s", arg);
                LOG_NONE("Valid levels: none, error, warn, info, debug, trace");
                break;
            }

            // Apply the new log level
            PCTerminal_SetLevel(level);

            // Confirm to the user that the level was successfully set
            LOG_NONE("Log level set to: %s", arg);
            break;
        }

        default:
        {
            // Handle unsupported or unknown commands
            LOG_WARN("Unsupported command");
            break;
        }
    }
}


/**
 * @brief Main debug command handler function.
 *
 * This function calls all sub-functions in the correct order:
 *  1. Acquire frame
 *  2. Decode and validate
 *  3. Dispatch system commands
 *
 * It should be called periodically in the main loop or a dedicated task.
 */
void command_handler_debug_process(void)
{ 
    // Step 1: Acquire frame
    if(!acquire_debug_frame())
        return;

    // Step 2: Decode frame and validate
    if(!acquire_and_decode_debug_message())
        return;

    // Step 3: Dispatch system/control command
    dispatch_system_command(&msg);
}
