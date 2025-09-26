/**
 * @file frame_handler_debug.c
 * @brief Frame handler for debug protocol (DBProtocol) using circular buffer with callback.
 *
 * Responsibilities:
 *  - Store complete frames received via IComm_Debug (event-driven, no polling)
 *  - Provide FIFO API for application
 *  - Independent from protocol decoding
 */

#include <string.h>
#include "i_protocol.h"
#include "i_frame_handler.h"
#include "i_comm.h"   // Needed for IComm_Debug

/* -------------------------------------------------------------------------- */
/*                           Configuration macros                             */
/* -------------------------------------------------------------------------- */

/** Maximum number of frames that can be stored in the circular buffer */
#define FRAME_HANDLER_DEBUG_MAX_FRAMES  16

/** Maximum allowed size (in bytes) for each frame */
#define FRAME_HANDLER_DEBUG_MAX_SIZE    64

/* -------------------------------------------------------------------------- */
/*                        Internal data structures                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Data structure representing a single debug frame.
 */
typedef struct {
    uint8_t data[FRAME_HANDLER_DEBUG_MAX_SIZE]; /**< Raw frame data */
    uint16_t length;                            /**< Length of valid data */
} debug_frame_t;

/** Circular buffer to store received frames */
static debug_frame_t frame_buffer[FRAME_HANDLER_DEBUG_MAX_FRAMES];

/** Write index (next free slot in the buffer) */
static volatile uint16_t head = 0;

/** Read index (next frame to be read) */
static volatile uint16_t tail = 0;

/* -------------------------------------------------------------------------- */
/*                        Internal helper functions                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief RX callback registered to IComm_Debug.
 * 
 * Called automatically by the communication driver when a complete frame is available.
 * The received frame is pushed into the circular buffer if space is available.
 * If the buffer is full, the frame is silently dropped.
 */
static void frame_handler_debug_on_frame_ready(void)
{
    uint8_t buffer[FRAME_HANDLER_DEBUG_MAX_SIZE];
    uint16_t len = 0;

    // Attempt to receive one frame from the debug communication driver
    if (IComm_Debug->receive(buffer, FRAME_HANDLER_DEBUG_MAX_SIZE) == COMM_OK)
    {
        // DBProtocol guarantees null-terminated frames, so strlen is valid
        len = strlen((char*)buffer);

        // Validate frame length
        if (len > 0 && len < FRAME_HANDLER_DEBUG_MAX_SIZE)
        {
            uint16_t next_head = (head + 1) % FRAME_HANDLER_DEBUG_MAX_FRAMES;

            // Only push the frame if the buffer is not full
            if (next_head != tail)
            {
                memcpy(frame_buffer[head].data, buffer, len);
                frame_buffer[head].length = len;
                head = next_head;
            }
            // else: buffer overflow -> frame dropped (could log or count drops)
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                        Public API functions                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Manually push a frame into the handler buffer.
 * 
 * @param[in] data    Pointer to the frame data.
 * @param[in] length  Length of the frame in bytes.
 * 
 * @retval true   Frame successfully stored.
 * @retval false  Buffer full or invalid length.
 */
bool frame_handler_debug_push(const uint8_t* data, uint16_t length)
{
    if (length == 0 || length > FRAME_HANDLER_DEBUG_MAX_SIZE)
        return false;

    uint16_t next_head = (head + 1) % FRAME_HANDLER_DEBUG_MAX_FRAMES;
    if (next_head == tail)
        return false; // Buffer full

    memcpy(frame_buffer[head].data, data, length);
    frame_buffer[head].length = length;
    head = next_head;
    return true;
}

/**
 * @brief Check if any frames are available in the buffer.
 * 
 * @retval true   At least one frame is available.
 * @retval false  Buffer is empty.
 */
bool frame_handler_debug_available(void)
{
    return head != tail;
}

/**
 * @brief Retrieve (pop) the oldest frame from the buffer.
 * 
 * @param[out] data    Pointer to the user buffer where frame data will be copied.
 * @param[out] length  Pointer to variable that will receive the frame length.
 * 
 * @retval true   Frame successfully retrieved.
 * @retval false  Buffer was empty.
 */
bool frame_handler_debug_pop(uint8_t* data, uint16_t* length)
{
    if (head == tail)
        return false; // Buffer empty

    if (data && length)
    {
        // Clear output buffer before copying
        memset(data, 0, strlen((char*)data));

        *length = frame_buffer[tail].length;
        memcpy(data, frame_buffer[tail].data, *length);
    }

    // Advance tail index (consume one frame)
    tail = (tail + 1) % FRAME_HANDLER_DEBUG_MAX_FRAMES;
    return true;
}

/**
 * @brief Flush the entire frame buffer (clear all pending frames).
 */
void frame_handler_debug_flush(void)
{
    head = tail = 0;
}

/**
 * @brief Initialize the debug frame handler.
 * 
 * Registers the RX callback to the debug communication interface. 
 * After calling this function, frames received via IComm_Debug will 
 * be automatically stored in the circular buffer.
 */
void DBFrameHandler_init(void)
{
    if (IComm_Debug && IComm_Debug->rx_callback)
    {
        IComm_Debug->rx_callback(frame_handler_debug_on_frame_ready);
    }
}

/* -------------------------------------------------------------------------- */
/*                 Global interface instance for application                  */
/* -------------------------------------------------------------------------- */

/** Static instance of the debug frame handler implementing the generic API */
const i_frame_handler_t FrameHandler_Debug = {
    .push       =   frame_handler_debug_push,
    .available  =   frame_handler_debug_available,
    .pop        =   frame_handler_debug_pop,
    .flush      =   frame_handler_debug_flush,
    .update     =   NULL   // Not needed in callback-driven implementation
};

/** Public pointer to the debug frame handler instance */
const i_frame_handler_t* DBFrameHandler = &FrameHandler_Debug;
