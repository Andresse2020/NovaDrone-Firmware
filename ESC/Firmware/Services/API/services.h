#ifndef SERVICES_INIT_H
#define SERVICES_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Types                                         */
/* -------------------------------------------------------------------------- */

typedef enum
{
    SERVICE_OK = 0,       // Operation successful
    SERVICE_ERROR,       // General error 
} service_status_t;


typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} log_level_t;

/* -------------------------------------------------------------------------- */
/*                          Public API                                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Configure the current log level filter.
 * Messages below this level will be ignored.
 */
void PCTerminal_SetLevel(log_level_t level);

/**
 * @brief Enable or disable ANSI color output in logs.
 *
 * @param enable true = enable colors, false = disable
 */
void PCTerminal_EnableColor(bool enable);

/**
 * @brief Core logging function (normally called via macros).
 */
void PCTerminal_Log(log_level_t level, const char* fmt, ...);

/**
 * @brief Initialize system core (HAL, clock)
 *
 * This function should be called once at system startup.
 * It initializes HAL and system clock.
 *
 * @return true if all initializations succeed, false otherwise
 */
service_status_t SSystem_Init(void);

/**
 * @brief Initialize all software services of the application.
 *
 * This function should be called during system startup to configure and initialize
 * the various service modules.
 */
service_status_t Services_Init(void);

/**
 * @brief Blink a specified LED.
 *
 * This function toggles the state of the given LED to provide a visual indication,
 * typically used for testing or status signaling.
 *
 * @param led The identifier of the LED to blink (see ILedId_t).
 */
void service_blink_status_Led(uint32_t delay);

/**
 * @brief Convert a float number to string for debug output.
 *
 * Works without printf/%f support. Handles negative numbers and
 * configurable decimal precision.
 *
 * @param value     Float value to convert
 * @param str       Destination buffer (must be large enough)
 * @param precision Number of digits after decimal point (e.g., 2 -> 1.23)
 */
void Service_FloatToString(float value, char* str, uint8_t precision);

/**
 * @brief Get the system running time in seconds.
 *
 * This function retrieves the current system tick count (usually in 
 * milliseconds, depending on the HAL/OS configuration) and converts 
 * it to seconds. It is a helper for timing/debug purposes.
 *
 * @return float Running time in seconds since system start-up.
 */
float Service_getRuningTime_second(void);

/**
 * @brief Convert current system tick (ms) into a formatted time string "XX min YY sec".
 * 
 * @param buffer    Pointer to a buffer where the formatted string will be written.
 * @param buf_size  Size of the buffer in bytes.
 * 
 * @note This function calls ITime->getTick() internally.
 *       Example output: "12 min 34 sec".
 */
void Service_GetRunTimeString(char *buffer, size_t buf_size);

/**
 * @brief Retrieve the current system clock frequency.
 *
 * @return uint32_t Current SYSCLK frequency in Hz.
 */
uint32_t Service_GetSysFrequencyMHz(void);

/**
 * @brief Perform a software reset of the MCU.
 *
 * This function triggers a complete system reset, equivalent
 * to a Power-On Reset. All registers and peripherals are
 * reset to their default state.
 *
 * Usage:
 *     Service_SystemReset();
 */
void Service_SystemReset(void);

/**
 * @brief Initialize the Data/Debug Frame Handler.
 * 
 * This function sets up the frame handler responsible for managing 
 * incoming communication frames (e.g., RX callbacks, parsing, and 
 * dispatching). It prepares the system to correctly handle and process 
 * communication data.
 */
void DBFrameHandler_init(void);

/**
 * @brief Release the communication test service.
 * 
 * This function is used for test communication purpose.
 */
void service_release_comm_test(void);


/**
 * @brief Get the internal MCU temperature in degrees Celsius.
 *
 * This service reads the internal temperature sensor through the ADC, 
 * applies calibration constants, and converts the raw value into a 
 * temperature expressed in °C.
 *
 * @note The accuracy depends on the calibration data provided in the device 
 *       memory (factory calibration values) and on the ADC configuration 
 *       (sampling time, resolution, Vref).
 *
 * @return Temperature value in degrees Celsius as a floating-point number.
 */
float Service_GetMCU_Temp(void);


/**
 * @brief Get the latest PCB temperature value
 * @return Current PCB temperature in °C
 *
 * @details Provides the most recently converted and processed
 *          PCB temperature measurement for external modules.
 */
float Service_GetPCB_Temp(void);

/**
 * @brief Retrieve the most recent bus voltage measurement
 * @return Bus voltage in volts (V), or 0.0f if unavailable
 */
float Service_GetBus_Voltage(void);

/**
 * @brief Retrieve the most recent 3.3V rail measurement
 * @return 3.3V rail voltage in volts (V), or 0.0f if unavailable
 */
float Service_Get3v3_Voltage(void);

/**
 * @brief Retrieve the most recent 12V rail measurement
 * @return 12V rail voltage in volts (V), or 0.0f if unavailable
 */
float Service_Get12V_Voltage(void);

/**
 * @file current_conversion.c
 * @brief Conversion utility: 12-bit ADC value → current (A)
 *
 * Formula:
 *      I(A) = (ADC_raw / 4095) * (Vref / (Gain * Rshunt))
 * 
 * Parameters:
 *      Vref   = 3.3 V
 *      Gain   = 20 V/V
 *      Rshunt = 10 mΩ
 * 
 * Result:
 *      1 LSB ≈ 4.028 mA
 */
float Service_ADC_To_Current(uint16_t adc_value);


// Update the local measurement buffer with the latest ADC readings
bool Service_ADC_Motor_UpdateMeasurements(void);

// Returns Phase A current in Amperes
float Service_Get_PhaseA_Current(void);

// Returns Phase B current in Amperes
float Service_Get_PhaseB_Current(void);

// Returns Phase C current in Amperes
float Service_Get_PhaseC_Current(void);

/**
 * @brief Command a DC motor between PHASE_A and PHASE_B.
 */
void Service_DC_Command_AB(float duty);

/**
 * @brief Command a DC motor between PHASE_B and PHASE_C.
 */
void Service_DC_Command_BC(float duty);

/**
 * @brief Command a DC motor between PHASE_C and PHASE_A.
 */
void Service_DC_Command_CA(float duty);

/**
 * @brief Stop all DC motor outputs (float all phases).
 */
void Service_DC_StopAll(void);

/* -------------------------------------------------------------------------- */
/*                          User-friendly macros                              */
/* -------------------------------------------------------------------------- */

#define LOG_NONE(fmt, ...)    PCTerminal_Log(LOG_LEVEL_NONE, fmt,  ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   PCTerminal_Log(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)    PCTerminal_Log(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)    PCTerminal_Log(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   PCTerminal_Log(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...)   PCTerminal_Log(LOG_LEVEL_TRACE, fmt, ##__VA_ARGS__)



/* -------------------------------------------------------------------------- */
/*                       User-friendly Commands Map                           */
/* -------------------------------------------------------------------------- */

/// System / Control commands
typedef enum {
    CMD_HELP        = 0x0001,   ///< Display list of available commands
    CMD_VERSION     = 0x0002,   ///< Print firmware version
    CMD_RESET       = 0x0003,   ///< Reset the system
    CMD_PING        = 0x0004,   ///< Ping / check alive
    CMD_STATUS      = 0x0005,   ///< General system status
    CMD_CLEAR       = 0x0006    ///< Clear the terminal
} system_cmd_t;

/// Logging / Debug commands
typedef enum {
    CMD_LOGLEVEL    = 0x0100,   ///< Set logging level (error, warn, info, debug)
    // CMD_LOGON       = 0x0101,   ///< Enable logging output
    // CMD_LOGOFF      = 0x0102,   ///< Disable logging output
    // CMD_TRACEON     = 0x0103,   ///< Enable trace/debug output
    // CMD_TRACEOFF    = 0x0104    ///< Disable trace/debug output
} debug_cmd_t;

// /// Communication / Interface commands
// typedef enum {
//     CMD_CONNECT     = 0x0200,   ///< Connect to a communication interface
//     CMD_DISCONNECT  = 0x0201,   ///< Disconnect from interface
//     CMD_SEND        = 0x0202,   ///< Send a raw frame or message
//     CMD_RECV        = 0x0203    ///< Receive last frame/message
// } comm_cmd_t;

// Project-specific commands
typedef enum {

    CMD_SETSPEED     = 0x1001,
    CMD_STOP         = 0x1002,
    CMD_GETCURRENT   = 0x1003,
    // CMD_MOVE         = 0x100x,
    // CMD_TAKE_CONTROL = 0x100x,
} project_cmd_t;


#ifdef __cplusplus
}
#endif

#endif // SERVICES_INIT_H