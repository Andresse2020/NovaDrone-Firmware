#ifndef I_PROTOCOL_H
#define I_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file i_protocol.h
 * @brief Generic protocol interface for all transport layers.
 *
 * This interface defines a **generic protocol abstraction**.
 * - Allows higher layers (control, application) to send/receive structured commands
 *   without knowing the underlying transport (UART, CAN, SPI, etc.)
 * - Supports multiple implementations:
 *   - ASCII/text for debug (terminal commands)
 *   - Binary for real-time operation
 *
 * Design principle:
 *   - Commands are identified by numeric IDs (`command_id`) for efficiency.
 *   - Argument list supports typed values (int, float, string).
 *   - Debug/logging may map IDs to human-readable strings internally.
 */

/* ------------------------------------------------------------------------- */
/* Argument types                                                            */
/* ------------------------------------------------------------------------- */

/**
 * @brief Types of arguments a command can carry.
 *
 * - ARG_INT: signed 32-bit integer
 * - ARG_FLOAT: 32-bit floating point
 * - ARG_STRING: null-terminated string (max 31 characters)
 *
 * Using typed arguments allows a single generic interface for both
 * text and binary protocols.
 */
typedef enum
{
    PROTOCOL_ARG_INT,     /**< 32-bit signed integer */
    PROTOCOL_ARG_FLOAT,   /**< 32-bit floating point */
    PROTOCOL_ARG_STRING   /**< String (null-terminated, ASCII) */
} protocol_arg_type_t;

/**
 * @brief Single command argument container.
 *
 * - `type` indicates which field of the union is valid.
 * - Union stores the actual value efficiently.
 */
typedef struct
{
    protocol_arg_type_t type; /**< Type of the argument */
    union
    {
        int32_t i;        /**< Integer value */
        float   f;        /**< Float value */
        char    str[32];  /**< String value (max 31 chars + null terminator) */
    } value;
} protocol_arg_t;

/* ------------------------------------------------------------------------- */
/* Protocol message structure                                                */
/* ------------------------------------------------------------------------- */

/**
 * @brief Generic protocol message.
 *
 * - `command_id` identifies the command (efficient for real-time systems).
 * - `args` stores the arguments with type information.
 * - `arg_count` tells how many arguments are valid.
 *
 * Notes:
 * - ASCII/debug implementations may map strings ↔ IDs internally.
 * - Binary implementations use command_id directly for high-speed processing.
 */
typedef struct
{
    uint16_t command_id;       /**< Unique numeric command identifier */
    protocol_arg_t args[8];    /**< Array of typed arguments (max 8) */
    size_t arg_count;          /**< Number of valid arguments in `args` */
} protocol_msg_t;

/* ------------------------------------------------------------------------- */
/* Protocol status codes                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief Status codes returned by protocol operations.
 *
 * - PROTOCOL_OK: operation succeeded
 * - PROTOCOL_ERROR: generic failure (e.g., buffer overflow)
 * - PROTOCOL_INVALID: message is malformed
 * - PROTOCOL_UNSUPPORTED: command is not supported by this implementation
 */
typedef enum
{
    PROTOCOL_OK = 0,
    PROTOCOL_ERROR,
    PROTOCOL_INVALID,
    PROTOCOL_UNSUPPORTED
} protocol_status_t;

/* ------------------------------------------------------------------------- */
/* Protocol interface                                                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Protocol interface structure.
 *
 * - Defines all functions any protocol implementation must provide.
 * - Allows higher layers to remain **independent of transport or encoding**.
 */
typedef struct
{
    /**
     * @brief Initialize the protocol.
     * @return true if initialization succeeded, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Encode a message into raw bytes for transport.
     * @param msg      Pointer to the message to encode
     * @param buffer   Output buffer for encoded data
     * @param max_len  Maximum buffer size in bytes
     * @param out_len  Pointer to store actual number of bytes written
     * @return PROTOCOL_OK if successful, error code otherwise
     *
     * Note: ASCII protocol may convert command_id → string.
     *       Binary protocol writes raw bytes directly.
     */
    protocol_status_t (*encode)(const protocol_msg_t* msg, uint8_t* buffer, size_t max_len, size_t* out_len);

    /**
     * @brief Decode raw bytes into a structured message.
     * @param buffer  Input buffer with raw data
     * @param length  Number of bytes in buffer
     * @param msg     Output message structure
     * @return PROTOCOL_OK if successful, error code otherwise
     *
     * Note: ASCII protocol parses strings and converts them to command_id + args.
     *       Binary protocol reads raw IDs and payload directly.
     */
    protocol_status_t (*decode)(const uint8_t* buffer, size_t length, protocol_msg_t* msg);

    /**
     * @brief Check if a command is supported.
     * @param command_id  Numeric command ID
     * @return true if command is supported, false otherwise
     *
     * Useful to validate incoming messages before dispatching.
     */
    bool (*is_supported)(uint16_t command_id);

    /**
     * @brief Get human-readable description of a command.
     * @param command_id  Numeric command ID
     * @return Pointer to static string describing the command, or NULL if unknown
     *
     * Typically used for logging or debug output.
     */
    const char* (*get_description)(uint16_t command_id);

    /**
     * @brief Show help or usage information for the protocol.
     *
     * - Implementation may print to console or log.
     * - Typically lists available commands and their parameters.
     */
    void (*show_help)(void);

} i_protocol_t;

/**
 * @brief Pointer to currently active protocol implementation.
 *
 * - Can point to any implementation matching `i_protocol_t` (ASCII, binary, etc.)
 * - Example usage:
 *     IProtocol->decode(buffer, length, &msg);
 */
extern const i_protocol_t* DBProtocol;

#ifdef __cplusplus
}
#endif

#endif /* I_PROTOCOL_H */
