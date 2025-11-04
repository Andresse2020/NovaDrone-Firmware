#include "service_generic.h"
#include "i_comm.h"   // Communication interface (e.g. UART, USB, etc.)
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                         Internal State                                     */
/* -------------------------------------------------------------------------- */

/// Current log level filter (default = INFO)
static log_level_t current_level = LOG_LEVEL_INFO;  

/// Enable/disable ANSI colors in logs (default = ON)
static bool color_enabled = true;                   

/* ANSI escape codes for terminal colors */
#define ANSI_RED        "\033[31m"      ///< Red   : errors
#define ANSI_YELLOW     "\033[33m"      ///< Yellow: warnings
#define ANSI_GREEN      "\033[32m"      ///< Green : info
#define ANSI_BLUE       "\033[34m"      ///< Blue  : debug
#define ANSI_GRAY       "\033[37m"      ///< Gray  : trace
#define ANSI_RESET      "\033[0m"       ///< Reset : restore default color
#define ANSI_BK_BLACK   "\033[40m"      ///< Black background
#define ANSI_WHITE      "\033[97m"      ///< White : no specific level

/* -------------------------------------------------------------------------- */
/*                         Public API                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set the minimum log level to display.
 *
 * Example: if set to LOG_LEVEL_WARN, only WARN and ERROR messages are shown.
 *
 * @param level Minimum log level (ERROR, WARN, INFO, DEBUG, TRACE)
 */
void PCTerminal_SetLevel(log_level_t level)
{
    current_level = level;
}

/**
 * @brief Enable or disable ANSI color output in logs.
 *
 * @param enable true = enable colors, false = disable
 */
void PCTerminal_EnableColor(bool enable)
{
    color_enabled = enable;
}

/**
 * @brief Core logging function (should not be called directly).
 *
 * Normally used through macros like LOG_ERROR(), LOG_WARN(), LOG_INFO(), etc.
 *
 * @param level Log level (ERROR, WARN, INFO, DEBUG, TRACE)
 * @param fmt   printf-style format string
 * @param ...   Arguments for the format string
 */
void PCTerminal_Log(log_level_t level, const char* fmt, ...)
{
    // Filter: ignore logs below current_level, or if no debug interface is available
    if (level > current_level || !IComm_Debug)
        return;

    // Choose prefix and color according to log level
    const char* prefix = "";
    const char* color  = "";

    if (color_enabled) {
        switch (level) {
            case LOG_LEVEL_NONE:  color = ANSI_WHITE;  prefix = ""      ; break;
            case LOG_LEVEL_ERROR: color = ANSI_RED;    prefix = "[ERR] "; break;
            case LOG_LEVEL_WARN:  color = ANSI_YELLOW; prefix = "[WRN] "; break;
            case LOG_LEVEL_INFO:  color = ANSI_GREEN;  prefix = "[INF] "; break;
            case LOG_LEVEL_DEBUG: color = ANSI_BLUE;   prefix = "[DBG] "; break;
            case LOG_LEVEL_TRACE: color = ANSI_GRAY;   prefix = "[TRC] "; break;
            default: break;
        }
    } else {
        // Same logic but without colors
        switch (level) {
            case LOG_LEVEL_NONE:  prefix = ""      ; break;
            case LOG_LEVEL_ERROR: prefix = "[ERR] "; break;
            case LOG_LEVEL_WARN:  prefix = "[WRN] "; break;
            case LOG_LEVEL_INFO:  prefix = "[INF] "; break;
            case LOG_LEVEL_DEBUG: prefix = "[DBG] "; break;
            case LOG_LEVEL_TRACE: prefix = "[TRC] "; break;
            default: break;
        }
    }

    // Format message using printf-style formatting
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    // If formatting failed or message is empty → exit
    if (msg_len < 0) return;

    // Send color code (if enabled)
    if (color_enabled) {
        IComm_Debug->send(NONE, (const uint8_t*)color, strlen(color));
    }

    // --------------------------------------------------------------------------
    // Erase the current prompt "> " on the terminal before sending new data
    // --------------------------------------------------------------------------
    // The sequence uses backspace, space, backspace to remove each character
    // from the terminal without leaving artifacts.
    //
    // Layout for each character:
    //   0x08  -> Backspace: move cursor left
    //   ' '   -> Overwrite character with space
    //   0x08  -> Move cursor back again
    //
    // For "> " (2 characters), we repeat this sequence twice.
    const char bs_prompt[] = {0x08, ' ', 0x08, 0x08, ' ', 0x08};
    IComm_Debug->send(NONE, (const uint8_t*)bs_prompt, sizeof(bs_prompt));


    // Send prefix (e.g. "[ERR] ") and the formatted message
    IComm_Debug->send(NONE, (const uint8_t*)prefix, strlen(prefix));
    IComm_Debug->send(NONE, (const uint8_t*)buffer, strlen(buffer));

    // Reset color (so the next line is not affected)
    if (color_enabled) {
        IComm_Debug->send(NONE, (const uint8_t*)ANSI_RESET, strlen(ANSI_RESET));
    }

    // Add carriage return + line feed
    const char crlf[] = "\r\n> ";
    IComm_Debug->send(NONE, (const uint8_t*)crlf, 4);
}