#ifndef I_COMM_H
#define I_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @file i_comm.h
 * @brief Interface for abstracting communication channels (CAN, UART, SPI, etc.).
 *
 * This interface defines a generic way to send and receive data
 * over any communication bus. It hides the underlying hardware details
 * so that higher layers (protocol, control) remain hardware-independent.
 */

/* -------------------------------------------------------------------------- */
/*                          Communication status codes                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Communication status codes.
 */
typedef enum
{
    COMM_OK = 0,        /**< Operation successful */
    COMM_ERROR,         /**< Generic error */
    COMM_TIMEOUT,       /**< Timeout occurred */
    COMM_BUSY           /**< Resource busy */
} comm_status_t;

/* -------------------------------------------------------------------------- */
/*                          Communication node                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Logical node for communication.
 *
 * Each endpoint corresponds to a specific device on the bus.
 * For example:
 *  - On CAN: it maps to a CAN ID.
 *  - On I2C: it maps to a slave address.
 *  - On UART/SPI: may be ignored or mapped to a logical device.
 *
 * The driver implementation performs the mapping internally
 * (typically via a switch-case).
 */
typedef enum
{
    NONE = 0,   /**< Invalid / unused node */
    DISPLAY,    /**< Display unit */
    // Extend as needed...
} comm_node_t;

/* -------------------------------------------------------------------------- */
/*                          Communication function type                       */
/* -------------------------------------------------------------------------- */

/**
 * @typedef rx_callback_t
 * @brief Function pointer type for RX complete callbacks.
 *
 * This callback is invoked by the communication driver when a new frame
 * has been received and is ready to be retrieved with `receive()`.
 *
 * Prototype:
 *     void callback(void);
 */
typedef void (*rx_callback_t)(void);



/* -------------------------------------------------------------------------- */
/*                          Communication interface                           */
/* -------------------------------------------------------------------------- */

// Callback type for RX complete event
typedef void (*rx_callback_t)(void);


/**
 * @brief Communication interface.
 */
typedef struct
{
    /**
     * @brief Initialize the communication peripheral.
     * @return true if successful, false otherwise.
     */
    bool (*init)(void);

    /**
     * @brief Send raw data buffer to a given endpoint.
     * @param node Node (destination device).
     * @param data Pointer to data buffer.
     * @param length Number of bytes to send.
     * @return COMM_OK if successful, error code otherwise.
     */
    comm_status_t (*send)(comm_node_t node, const uint8_t* data, uint16_t length);

    /**
     * @brief Receive raw data buffer from a given endpoint.
     * Depending on the bus, the endpoint may or may not be relevant.
     * @param ep Endpoint (source device).
     * @param data Pointer to buffer where received data will be stored.
     * @param length Number of bytes to receive.
     * @return COMM_OK if successful, error code otherwise.
     */
    comm_status_t (*receive)(uint8_t* data, uint16_t length);

    /**
     * @brief Check if the transmitter is ready for a new send.
     * @return true if TX is idle, false if busy.
     */
    bool (*tx_ready)(void);

    /**
     * @brief Check if the receiver has completed a reception.
     * @return true if RX buffer contains valid data, false otherwise.
     */
    bool (*rx_available)(void);

    /**
     * @brief Flush or reset the communication channel.
     * This can clear buffers or reset the peripheral state.
     */
    void (*flush)(void);

    /** Optional: register callback called when a frame is ready */
    void     (*rx_callback)(rx_callback_t cb);

} i_comm_t;

/* -------------------------------------------------------------------------- */
/*                              Global instance                               */
/* -------------------------------------------------------------------------- */

extern const i_comm_t* IComm_Debug;  /**< Global communication interface instance for debugging */
extern const i_comm_t* IComm_Release;/**< Global communication interface instance for release */

#ifdef __cplusplus
}
#endif

#endif /* I_COMM_H */
