/**
 * @file protocol_ascii.c
 * @brief ASCII/Text debug protocol implementation.
 *
 * This implementation of i_protocol_t decodes/encodes human-readable
 * commands from/to a terminal or debug interface.
 *
 * - Input: commands sent as ASCII strings (e.g. "setspeed 50\n")
 * - Output: optional echo or logging in ASCII
 * - Each message is mapped to a numeric command_id internally for efficiency
 */

#include <stdlib.h>
#include <ctype.h>

#include "services.h"
#include "protocol.h"
#include "i_comm.h"

/* ------------------------------------------------------------------------- */
/* Internal mapping: string <-> command ID                                   */
/* ------------------------------------------------------------------------- */

/**
 * @brief Mapping of ASCII command names to internal command IDs.
 *
 * Used only inside this implementation.
 */
typedef struct {
    const char* name;        ///< ASCII command name
    uint16_t command_id;     ///< Numeric command ID
    const char* description; ///< What the command does
    const char* params;      ///< Parameters (optional, e.g. "<speed:int>")
} ascii_command_map_t;


/**
 * @file ascii_command_map.h
 * @brief Generic ASCII command to internal command ID mapping
 *
 * This table maps human-readable ASCII commands to unique 16-bit command IDs.
 * Designed to be extendable for project-specific commands.
 */

const ascii_command_map_t command_map[] = {
    // ---------------------------------------------------------------------
    // System / Control commands
    // ---------------------------------------------------------------------
    {"help",      CMD_HELP,    "Display list of available commands", "[none]"},
    {"version",   CMD_VERSION, "Print firmware version",             "[none]"},
    {"reset",     CMD_RESET,   "Reset the system",                   "[none]"},
    {"ping",      CMD_PING,    "Check system is alive",              "[none]"},
    {"status",    CMD_STATUS,  "General system status",              "[none]"},
    {"clear",     CMD_CLEAR,   "Clear the terminal screen",          "[none]"},

    // ---------------------------------------------------------------------
    // Logging / Debug commands
    // ---------------------------------------------------------------------
    {"loglevel",  CMD_LOGLEVEL,"Set logging level",                  "<level:str>"},
    // {"logon",     CMD_LOGON,   "Enable logging output",              "[none]"},
    // {"logoff",    CMD_LOGOFF,  "Disable logging output",             "[none]"},
    // {"traceon",   CMD_TRACEON, "Enable trace/debug output",          "[none]"},
    // {"traceoff",  CMD_TRACEOFF,"Disable trace/debug output",         "[none]"},

    // ---------------------------------------------------------------------
    // Communication / Interface commands
    // ---------------------------------------------------------------------
    // {"connect",   CMD_CONNECT, "Connect to a communication interface","<ifname:str>"},
    // {"disconnect",CMD_DISCONNECT,"Disconnect from interface",        "[none]"},
    // {"send",      CMD_SEND,    "Send a raw frame/message",           "<data:hex>"},
    // {"recv",      CMD_RECV,    "Receive last frame/message",         "[none]"},

    // ---------------------------------------------------------------------
    // Project-specific commands
    // ---------------------------------------------------------------------
    // {"setspeed",    CMD_SETSPEED,   "Set actuator speed",                       "<speed:int>"},
    // {"move",        CMD_MOVE,       "Move actuator to position",                "<pos:int>"},
    // {"take_control",CMD_TAKE_CONTROL,"Take manual control of the system",       "[none]"}
};

/* ------------------------------------------------------------------------- */
/* Size of the commands map                                                  */
/* ------------------------------------------------------------------------- */

#define COMMAND_MAP_SIZE (sizeof(command_map)/sizeof(command_map[0]))

/* ------------------------------------------------------------------------- */
/* Helper: map command name -> ID                                             */
/* ------------------------------------------------------------------------- */

/**
 * @brief Convert an ASCII command string to its numeric command ID.
 *
 * @param name Null-terminated command string
 * @return command ID if found, 0 otherwise
 */
static uint16_t ascii_command_to_id(const char* name)
{
    if (name == NULL)
        return 0;

    for (size_t i = 0; i < COMMAND_MAP_SIZE; i++)
    {
        if (strcmp(command_map[i].name, name) == 0)
            return command_map[i].command_id;
    }
    return 0; // unknown command
}

/* ------------------------------------------------------------------------- */
/* Helper: remove trailing newline and carriage return                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Remove '\n' and '\r' at the end of a string in-place.
 *
 * @param str Null-terminated string (mutable)
 */
static void trim_newline(char* str)
{
    if (str == NULL) return;

    char* end = str + strlen(str) - 1;
    while (end >= str && (*end == '\n' || *end == '\r'))
    {
        *end = '\0';
        end--;
    }
}

/* ------------------------------------------------------------------------- */
/* Helper: Check if a string is a valid float                               */
/* ------------------------------------------------------------------------- */

/**
 * @brief Check if a string is a valid float.
 * @param token Input string.
 * @return true if token is a float, false otherwise.
 */
static bool is_float(const char *token)
{
    size_t i = 0;
    bool has_dot = false;
    bool has_digit = false;

    // Optional sign
    if ((token[0] == '-' || token[0] == '+') && token[1] != '\0')
        i = 1;

    for (; token[i] != '\0'; i++)
    {
        if (isdigit((unsigned char)token[i]))
        {
            has_digit = true;
        }
        else if (token[i] == '.')
        {
            if (has_dot) return false; // multiple dots → invalid
            has_dot = true;
        }
        else
        {
            return false; // invalid character
        }
    }

    return has_dot && has_digit; // must have at least one dot and one digit
}


/* ------------------------------------------------------------------------- */
/* Helper: parse a single argument from string                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Convert a token string to a typed protocol_arg_t.
 *
 * Rules:
 * - Only digits (optionally starting with '-') → int
 * - Contains '.' → float
 * - Otherwise → string
 *
 * @param token Null-terminated input token
 * @param arg   Output argument structure (filled)
 */
static void parse_argument(const char* token, protocol_arg_t* arg)
{
    if (!token || !arg) return;

    // Check for integer
    bool is_int = true;
    size_t i = 0;
    if (token[0] == '-' && token[1] != '\0') i = 1;
    for (; token[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)token[i]))
        {
            is_int = false;
            break;
        }
    }

    if (is_int)
    {
        arg->type = PROTOCOL_ARG_INT;
        arg->value.i = atoi(token);
        return;
    }

    // Check for float
    if (is_float(token))
    {
        arg->type = PROTOCOL_ARG_FLOAT;
        arg->value.f = (float)atof(token);
        return;
    }

    // Otherwise string
    arg->type = PROTOCOL_ARG_STRING;
    strncpy(arg->value.str, token, sizeof(arg->value.str) - 1);
    arg->value.str[sizeof(arg->value.str) - 1] = '\0';
}

/* ------------------------------------------------------------------------- */
/* Helper: convert a single argument to ASCII string                          */
/* ------------------------------------------------------------------------- */

/**
 * @brief Convert a single protocol_arg_t to string.
 *
 * @param arg   Input argument
 * @param buffer Output buffer to write string
 * @param buf_size Size of output buffer in bytes
 */
static void argument_to_string(const protocol_arg_t* arg, char* buffer, size_t buf_size)
{
    if (!arg || !buffer || buf_size == 0) return;

    switch (arg->type)
    {
        case PROTOCOL_ARG_INT:
            snprintf(buffer, buf_size, "%ld", arg->value.i);
            break;

        case PROTOCOL_ARG_FLOAT:
            snprintf(buffer, buf_size, "%.6f", arg->value.f);
            break;

        case PROTOCOL_ARG_STRING:
            strncpy(buffer, arg->value.str, buf_size - 1);
            buffer[buf_size - 1] = '\0';
            break;

        default:
            buffer[0] = '\0'; // unknown type
            break;
    }
}


/* ------------------------------------------------------------------------- */
/* Protocol API: init                                                         */
/* ------------------------------------------------------------------------- */

/**
 * @brief Initialize the ASCII/debug protocol.
 *
 * This function sets up any internal state or buffers.
 * Currently, no state is needed, so it simply returns true.
 *
 * @return Always true (initialization successful)
 */
static bool ascii_init(void)
{
    // For future extension: allocate buffers or reset state
    return true;
}


/* ------------------------------------------------------------------------- */
/* Protocol API: decode ASCII                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Decode ASCII command string into a protocol_msg_t.
 *
 * Steps:
 * 1. Copy input buffer to temporary string (null-terminated)
 * 2. Trim newline/carriage return
 * 3. Tokenize string by spaces
 * 4. First token → command name → map to command_id
 * 5. Remaining tokens → parse into arguments
 *
 * Example input: "setspeed 50 100\n"
 * Output:
 *   msg->command_id = 0x0001
 *   msg->args[0].i = 50
 *   msg->args[1].i = 100
 *   msg->arg_count = 2
 *
 * @param buffer Input raw ASCII buffer
 * @param length Length of input buffer
 * @param msg    Output structured message
 * @return PROTOCOL_OK if successful, PROTOCOL_ERROR/PROTOCOL_INVALID otherwise
 */
static protocol_status_t ascii_decode(const uint8_t* buffer, size_t length, protocol_msg_t* msg)
{
    if (!buffer || !msg || length == 0) return PROTOCOL_INVALID;

    // Copy input into temporary buffer to ensure null-terminated string
    char temp[64];
    if (length >= sizeof(temp)) return PROTOCOL_ERROR; // input too long
    memcpy(temp, buffer, length);
    temp[length] = '\0';

    // Remove trailing '\n' or '\r'
    trim_newline(temp);

    // Tokenize string by spaces
    char* saveptr = NULL;
    char* token = strtok_r(temp, " ", &saveptr);

    if (!token) return PROTOCOL_INVALID;

    // Map command name → command_id
    uint16_t id = ascii_command_to_id(token);
    if (id == 0) return PROTOCOL_UNSUPPORTED;

    msg->command_id = id;

    // Parse remaining tokens as arguments
    msg->arg_count = 0;
    while ((token = strtok_r(NULL, " ", &saveptr)) != NULL)
    {
        if (msg->arg_count >= (sizeof(msg->args)/sizeof(msg->args[0])))
            break; // max arguments reached

        parse_argument(token, &msg->args[msg->arg_count]);
        msg->arg_count++;
    }

    return PROTOCOL_OK;
}

/* ------------------------------------------------------------------------- */
/* Protocol API: encode ASCII                                                 */
/* ------------------------------------------------------------------------- */

/**
 * @brief Encode a protocol_msg_t into an ASCII string.
 *
 * Format:
 *   "command arg1 arg2 ... argN\n"
 *
 * Example:
 *   msg->command_id = 1 ("setspeed")
 *   msg->args[0].i = 50
 *   Output: "setspeed 50\n"
 *
 * @param msg      Input structured message
 * @param buffer   Output buffer for ASCII string
 * @param max_len  Maximum size of output buffer
 * @param out_len  Pointer to store actual number of bytes written
 * @return PROTOCOL_OK if successful, error code otherwise
 */
static protocol_status_t ascii_encode(const protocol_msg_t* msg, uint8_t* buffer, size_t max_len, size_t* out_len)
{
    if (!msg || !buffer || !out_len || max_len == 0)
        return PROTOCOL_ERROR;

    // Lookup command name from ID
    const char* cmd_name = NULL;
    for (size_t i = 0; i < COMMAND_MAP_SIZE; i++)
    {
        if (command_map[i].command_id == msg->command_id)
        {
            cmd_name = command_map[i].name;
            break;
        }
    }

    if (!cmd_name) return PROTOCOL_UNSUPPORTED;

    // Start building output string
    size_t used = 0;
    int n = snprintf((char*)buffer + used, max_len - used, "%s", cmd_name);
    if (n < 0 || (size_t)n >= max_len - used) return PROTOCOL_ERROR;
    used += (size_t)n;

    // Encode arguments
    for (size_t i = 0; i < msg->arg_count; i++)
    {
        char arg_str[32];
        argument_to_string(&msg->args[i], arg_str, sizeof(arg_str));

        // Add space separator
        if (used + 1 >= max_len) return PROTOCOL_ERROR;
        buffer[used++] = ' ';

        // Append argument string
        size_t arg_len = strlen(arg_str);
        if (used + arg_len >= max_len) return PROTOCOL_ERROR;
        memcpy(buffer + used, arg_str, arg_len);
        used += arg_len;
    }

    // Terminate with newline and return
    if (used + 1 >= max_len) return PROTOCOL_ERROR;
    buffer[used++] = '\r';
    buffer[used++] = '\n';

    *out_len = used;
    return PROTOCOL_OK;
}


/* ------------------------------------------------------------------------- */
/* Protocol API: check if command is supported                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Check if a command ID is supported by this ASCII protocol.
 *
 * @param command_id Numeric command ID
 * @return true if command exists in command_map, false otherwise
 *
 * Used to quickly validate incoming messages before dispatching.
 */
static bool ascii_is_supported(uint16_t command_id)
{
    for (size_t i = 0; i < COMMAND_MAP_SIZE; i++)
    {
        if (command_map[i].command_id == command_id)
            return true;
    }
    return false;
}


/* ------------------------------------------------------------------------- */
/* Protocol API: get command description                                      */
/* ------------------------------------------------------------------------- */

/**
 * @brief Get human-readable description of a command.
 *
 * - Returns the ASCII command name for logging/debugging.
 * - Returns NULL if command ID is unknown.
 *
 * @param command_id Numeric command ID
 * @return Pointer to static string, or NULL if unknown
 */
static const char* ascii_get_description(uint16_t command_id)
{
    for (size_t i = 0; i < COMMAND_MAP_SIZE; i++)
    {
        if (command_map[i].command_id == command_id)
            return command_map[i].name;
    }
    return NULL;
}


/* ------------------------------------------------------------------------- */
/* Display all available commands with descriptions and parameters.          */
/* ------------------------------------------------------------------------- */

#define TERMINAL_WIDTH 80   ///< Maximum width of terminal line
#define COL_CMD   12        ///< Width of "Command" column
#define COL_DESC  40        ///< Width of "Description" column
#define COL_PARAM 20        ///< Width of "Params" column

/**
 * @brief Send a string over the debug interface.
 *
 * Wraps the actual IComm_Debug->send() call for convenience.
 *
 * @param str Null-terminated string to send
 */
static void dbg_send(const char* str)
{
    // Send using the 3-argument API
    IComm_Debug->send(NONE, (uint8_t*)str, strlen(str));
}

/**
 * @brief Print a single command line with automatic wrapping.
 *
 * - cmd: command name (only printed on the first line)
 * - desc: command description (wraps if longer than COL_DESC)
 * - params: parameter description (wraps if longer than COL_PARAM)
 *
 * Each wrapped line aligns properly under the columns.
 */
static void print_wrapped(const char* cmd, const char* desc, const char* params)
{
    char buffer[128];          ///< Temporary line buffer
    size_t desc_len = strlen(desc);
    size_t param_len = strlen(params);
    size_t offset_desc = 0, offset_param = 0;

    do {
        // Format a line of the table
        // %-*s: left-align column, width = column size
        // %-*.*s: left-align with max width to prevent overflow
        snprintf(buffer, sizeof(buffer), " %-*s | %-*.*s | %-*.*s\r\n",
                 COL_CMD, (offset_desc == 0 && offset_param == 0) ? cmd : "",   // Only print command on first line
                 COL_DESC, COL_DESC, desc + offset_desc,                          // Print chunk of description
                 COL_PARAM, COL_PARAM, params + offset_param);                    // Print chunk of parameters
        dbg_send(buffer);

        // Move offsets forward
        offset_desc  += (desc_len  > COL_DESC)  ? COL_DESC  : desc_len;
        offset_param += (param_len > COL_PARAM) ? COL_PARAM : param_len;
    } while (offset_desc < desc_len || offset_param < param_len);  // Repeat until everything printed
}

/**
 * @brief Display all available commands in a professional, tabular format.
 *
 * - Shows headers and separators
 * - Wraps long descriptions and parameters
 * - Adds a prompt "> " at the end
 */
static void ascii_show_help(void)
{
    char buffer[256];

    // 0. Erase the current prompt "> " on the terminal before sending new data
    const char bs_prompt[] = {0x08, ' ', 0x08, 0x08, ' ', 0x08};
    dbg_send(bs_prompt);

    // 1. Header
    dbg_send("\r\n============================ Available Commands ============================\r\n\n");

    // 2. Table header
    snprintf(buffer, sizeof(buffer), " %-*s | %-*s | %-*s\r\n",
             COL_CMD, "Command",
             COL_DESC, "Description",
             COL_PARAM, "Params");
    dbg_send(buffer);

    // 3. Separator line (full width)
    for (int i = 0; i < TERMINAL_WIDTH; i++) buffer[i] = '-';
    buffer[TERMINAL_WIDTH] = '\0';
    strncat(buffer, "\r\n", sizeof(buffer) - strlen(buffer) - 1);
    dbg_send(buffer);

    // 4. Print all commands
    for (size_t i = 0; i < sizeof(command_map)/sizeof(command_map[0]); i++) {
        print_wrapped(command_map[i].name,
                      command_map[i].description,
                      command_map[i].params);
    }

    // 5. Footer line + prompt
    for (int i = 0; i < TERMINAL_WIDTH; i++) buffer[i] = '-';
    buffer[TERMINAL_WIDTH] = '\0';
    strncat(buffer, "\r\n> ", sizeof(buffer) - strlen(buffer) - 1);
    dbg_send(buffer);
}






/* ------------------------------------------------------------------------- */
/* Protocol instance (global)                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Global instance of debug protocol implementation.
 *
 * Higher layers can access this via `IProtocol`.
 */
const i_protocol_t protocol_debug = {
    .init               =   ascii_init,
    .encode             =   ascii_encode,
    .decode             =   ascii_decode,
    .is_supported       =   ascii_is_supported,
    .get_description    =   ascii_get_description,
    .show_help          =   ascii_show_help
};

/* Set this pointer at startup to select debug protocol */
const i_protocol_t* DBProtocol = &protocol_debug;