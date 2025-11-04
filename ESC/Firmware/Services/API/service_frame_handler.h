/**
 * @file i_frame_handler.h
 * @brief Generic interface for frame handler modules (FIFO-based).
 *
 * A frame handler is responsible for:
 *  - Receiving validated frames from a protocol layer
 *  - Storing them in a circular buffer (FIFO)
 *  - Providing functions to check, read, and flush frames
 *
 * This interface allows different protocols (e.g. Debug, CAN, etc.)
 * to reuse the same abstraction.
 */

#ifndef I_FRAME_HANDLER_H
#define I_FRAME_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*                          Interface type definition                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Frame handler interface.
 *
 * Each implementation must provide concrete functions
 * for these operations.
 */
typedef struct
{
    /**
     * @brief Push a frame into the handler buffer.
     *
     * @param data Pointer to frame data
     * @param length Frame length in bytes
     * @return true if pushed successfully, false if buffer full or invalid
     */
    bool (*push)(const uint8_t* data, uint16_t length);

    /**
     * @brief Check if a frame is available to read.
     *
     * @return true if at least one frame is ready
     */
    bool (*available)(void);

    /**
     * @brief Pop the oldest frame from the buffer.
     *
     * @param data Destination buffer (must be large enough)
     * @param length Pointer to receive frame length
     * @return true if frame returned, false if buffer empty
     */
    bool (*pop)(uint8_t* data, uint16_t* length);

    /**
     * @brief Flush (clear) all stored frames.
     */
    void (*flush)(void);

    /**
     * @brief Optional update function.
     *        Typically polls the underlying protocol for new frames.
     *        Can be called periodically in main loop / task.
     */
    void (*update)(void);

} i_frame_handler_t;


extern const i_frame_handler_t* DBFrameHandler;


#ifdef __cplusplus
}
#endif

#endif /* I_FRAME_HANDLER_H */
