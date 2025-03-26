#ifndef __LINK_LAYER_HPP__
#define __LINK_LAYER_HPP__

#include "layer.hpp"
#include "config.hpp"
#include "crc16.hpp"
#include "cobs.hpp"
#include "physical_layer.hpp"

namespace robust_serial
{

// Link Layer Frame Structure
#define LINK_HEADER_SIZE      2                                                        // type(1) + length(1)
#define LINK_CRC_SIZE         2                                                        // CRC16 size in bytes
#define LINK_MIN_FRAME_SIZE   (LINK_HEADER_SIZE + LINK_CRC_SIZE)                       // 4 bytes
#define LINK_MAX_FRAME_SIZE   COBS_MAX_BLOCK_SIZE                                      // 254 bytes
#define LINK_MAX_PAYLOAD_SIZE (COBS_MAX_BLOCK_SIZE - LINK_HEADER_SIZE - LINK_CRC_SIZE) // 250 bytes

#define LINK_OUTGOING_BUFFER_SIZE (COBS_MAX_ENCODED_SIZE * 2)
#define LINK_INCOMING_BUFFER_SIZE (COBS_MAX_ENCODED_SIZE * 2)

// Frame Types
#define LINK_FRAME_TYPE_DATA 0x01 // Data frame type

/**
 * @brief Link Layer States
 */
enum LinkState
{
    LINK_STATE_READY = 0,   // Ready to send/receive frames
    LINK_STATE_SENDING = 1, // Currently sending a frame
    LINK_STATE_ERROR = 2    // Error state
};

/**
 * @brief Error codes specific to the Link Layer
 *
 * These error codes are in the link layer error range (-64 to -33).
 * Each error code should be unique within this range.
 */
enum LinkLayerError
{
    LINK_SUCCESS = LAYER_SUCCESS,
    LINK_ERROR_GENERAL = ERROR_RANGE_LINK,            // Generic error
    LINK_ERROR_INVALID_FRAME = ERROR_RANGE_LINK - 1,  // Invalid frame format
    LINK_ERROR_CRC_ERROR = ERROR_RANGE_LINK - 2,      // CRC check failed
    LINK_ERROR_BUFFER_FULL = ERROR_RANGE_LINK - 3,    // Buffer is full
    LINK_ERROR_TIMEOUT = ERROR_RANGE_LINK - 4,        // Operation timeout
    LINK_ERROR_INVALID_PARAM = ERROR_RANGE_LINK - 5,  // Invalid parameter
    LINK_ERROR_NOT_INITIALIZED = ERROR_RANGE_LINK - 6 // Layer not initialized
};

/**
 * @brief Events reported by Link Layer
 */
enum LinkLayerEvent
{
    LINK_LAYER_EVENT_READY = 0,               // Link is ready for transmission
    LINK_LAYER_EVENT_FRAME_SENT,              // Frame was sent successfully
    LINK_LAYER_EVENT_FRAME_RECEIVED,          // Frame received successfully
    LINK_LAYER_EVENT_CRC_ERROR,               // Frame CRC validation failed
    LINK_LAYER_EVENT_ERROR,                   // General link error
    LINK_LAYER_EVENT_OUTGOING_DATA_AVAILABLE, // New data available in outgoing buffer
    LINK_LAYER_EVENT_INCOMING_DATA_AVAILABLE  // New data available in incoming buffer
};

/**
 * @brief Link Layer implementation providing frame integrity.
 *
 * The Link Layer is responsible for ensuring frame integrity between peers.
 * It handles frame construction, CRC checking, and COBS encoding/decoding.
 * This layer is connection-agnostic and focuses solely on frame-level integrity.
 *
 * Key responsibilities:
 * - Frame construction and parsing
 * - Error detection using CRC16
 * - COBS encoding/decoding for frame delimiting
 * - Event-based data flow control
 *
 * Data Flow:
 * Outgoing:
 * 1. Upper layer calls send() to queue data
 * 2. Layer reports LINK_LAYER_EVENT_OUTGOING_DATA_AVAILABLE
 * 3. Event handler calls process_outgoing_data()
 * 4. Layer sends data and reports LINK_LAYER_EVENT_FRAME_SENT
 *
 * Incoming:
 * 1. Physical layer calls on_receive() to queue data
 * 2. Layer reports LINK_LAYER_EVENT_INCOMING_DATA_AVAILABLE
 * 3. Event handler calls process_incoming_data()
 * 4. Layer processes data and reports LINK_LAYER_EVENT_FRAME_RECEIVED
 *
 * Frame Structure (before COBS encoding):
 * +------------+-------------+-----------------+------------+
 * | TYPE (1B)  | LENGTH (1B) | PAYLOAD (0-nB)  | CRC16 (2B) |
 * |  0x01      |   0-250     |    data...      |            |
 * +------------+-------------+-----------------+------------+
 *
 * Frame Types:
 * Current:
 * - DATA (0x01): Contains payload data
 *
 * Reserved for Future Use:
 * - CONTROL (0x02): Link control frames (e.g., link configuration, status)
 * - DIAGNOSTIC (0x03): Link diagnostic information
 * - KEEP_ALIVE (0x04): Keep-alive frames
 * - FLOW_CONTROL (0x05): Flow control frames
 * - RESET (0x06): Link reset request/response
 * - ERROR (0x07): Link error reporting
 *
 * Notes:
 * - All frames are COBS encoded before transmission
 * - FRAME_DELIMITER (0x00) is added after COBS encoding
 * - Maximum payload size is 250 bytes (254 - 4 bytes overhead)
 * - Total frame size before encoding is 254 bytes (1 + 1 + 250 + 2)
 * - CRC16 is calculated over all preceding bytes
 * - Uses event-driven architecture for data flow control
 */
class LinkLayer : public Layer
{
public:
    LinkLayer();
    virtual ~LinkLayer();

    /**
     * @brief Initialize the link layer
     *
     * This should be called after construction to properly initialize the layer
     * and report the initial ready state to upper layers.
     */
    virtual void initialize();
    virtual void deinitialize();

    /**
     * @brief Sends a frame through the physical layer.
     *
     * The frame is constructed with:
     * - Frame type (DATA)
     * - Payload length
     * - CRC16 checksum
     * - COBS encoding
     *
     * @param data Pointer to the data to send
     * @param length Length of the data in bytes
     * @return LINK_SUCCESS on success, negative error code on failure
     */
    virtual int send(const uint8_t *data, uint16_t length);

    /**
     * @brief Processes received data from the physical layer.
     *
     * Handles:
     * - Frame delimiting
     * - COBS decoding
     * - CRC validation
     * - Data forwarding to upper layer
     */
    virtual int on_receive(const uint8_t *data, uint16_t length);

    virtual uint16_t get_max_payload_size() const
    {
        return LINK_MAX_PAYLOAD_SIZE;
    }

    /**
     * @brief Resets the link layer state.
     *
     * Resets:
     * - State machine
     * - Buffers
     */
    void reset();

    /**
     * @brief Check if a payload size is valid
     *
     * Validates that the payload size is within the maximum allowed size
     * (LINK_MAX_PAYLOAD_SIZE) to ensure the total frame size fits within COBS block size.
     *
     * @param length Length of the payload in bytes
     * @return true if the payload size is valid, false otherwise
     */
    static bool is_valid_payload_size(uint16_t length)
    {
        return length <= LINK_MAX_PAYLOAD_SIZE;
    }

    // These functions are called by the layer manager to process data
    int process_outgoing_data();
    int process_incoming_data();

private:
    bool validate_frame(const uint8_t *frame, uint16_t length);

    // Transmit buffers
    uint8_t frame_buffer_[LINK_MAX_FRAME_SIZE];     // Buffer for frame construction
    uint8_t encoded_buffer_[COBS_MAX_ENCODED_SIZE]; // Buffer for COBS encoded frame
    uint16_t encoded_length_;                       // Length of encoded frame

    uint8_t decode_buffer_[LINK_MAX_FRAME_SIZE]; // Buffer for COBS decoded frame

    uint8_t outgoing_buffer_[LINK_OUTGOING_BUFFER_SIZE];
    uint16_t outgoing_buffer_length_;

    uint8_t incoming_buffer_[LINK_INCOMING_BUFFER_SIZE];
    uint16_t incoming_buffer_length_;

    // Prevent copy and assignment
    LinkLayer(const LinkLayer &);
    LinkLayer &operator=(const LinkLayer &);
};

} // namespace robust_serial

#endif // __LINK_LAYER_HPP__
