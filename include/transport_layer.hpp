#pragma once

#include "layer.hpp"
#include "config.hpp"
#include "link_layer.hpp"   // For LINK_MAX_PAYLOAD_SIZE
#include "system_utils.hpp" // For get_current_time_ms

namespace robust_serial
{

/**
 * @brief Transport Layer Packet Types
 *
 * These define the different types of packets that can be sent/received
 * at the transport layer level.
 */
#define TRANSPORT_PACKET_TYPE_SYN          0x01 /**< Connection request packet (SYN) */
#define TRANSPORT_PACKET_TYPE_SYN_ACK      0x02 /**< Connection acknowledgment packet (SYN-ACK) */
#define TRANSPORT_PACKET_TYPE_ACK          0x03 /**< Acknowledgment packet */
#define TRANSPORT_PACKET_TYPE_FIN          0x04 /**< Disconnection request packet (FIN) */
#define TRANSPORT_PACKET_TYPE_FIN_ACK      0x05 /**< Disconnection acknowledgment packet (FIN-ACK) */
#define TRANSPORT_PACKET_TYPE_DATA         0x06 /**< Data packet */
#define TRANSPORT_PACKET_TYPE_DATA_ACK     0x07 /**< Data acknowledgment packet */
#define TRANSPORT_PACKET_TYPE_DATA_NACK    0x08 /**< Data negative acknowledgment packet */
#define TRANSPORT_PACKET_TYPE_KEEPALIVE    0x09 /**< Keep-alive request packet */
#define TRANSPORT_PACKET_TYPE_KEEPALIVE_ACK 0x0A /**< Keep-alive acknowledgment packet */
#define TRANSPORT_PACKET_TYPE_DATAGRAM     0x0B /**< Datagram packet (connectionless) */
#define TRANSPORT_PACKET_TYPE_MAX          0x0C /**< Maximum value for packet types */

// Connection ID related definitions
#define TRANSPORT_CONNECTION_ID_INVALID    0x00 /**< Invalid connection ID */
#define TRANSPORT_CONNECTION_ID_MAX        0xFF /**< Maximum connection ID value */
#define TRANSPORT_CONNECTION_ID_START      0x01 /**< Starting connection ID value */

/**
 * @brief Transport Layer Packet Structure
 *
 * The transport layer provides two types of packets:
 * 1. Connection-oriented packets (with sequence numbers and connection ID)
 * 2. Connectionless datagram packets
 *
 * 1. Connection-oriented Packet Format:
 *    +----------------+----------------+----------------+----------------+------------------+
 *    | Packet Type    | Conn ID        | Seq Number     | Payload Length | Payload Data     |
 *    | (1 byte)       | (1 byte)       | (1 byte)       | (1 byte)       | (0-246 bytes)    |
 *    | 0x01-0x0A      | 0x01-0xFF      | 0-255          | 0-246          |                  |
 *    +----------------+----------------+----------------+----------------+------------------+
 *
 *    Special cases:
 *    ACK Packet:
 *    +----------------+----------------+----------------+----------------+
 *    | Packet Type    | Conn ID        | Seq Number     | Zero          |
 *    | (1 byte)       | (1 byte)       | (1 byte)       | (1 byte)      |
 *    | 0x03/0x07      | 0x01-0xFF      | 0-255          | 0x00          |
 *    +----------------+----------------+----------------+----------------+
 *
 *    Keep-alive Packet:
 *    +----------------+----------------+----------------+----------------+
 *    | Packet Type    | Conn ID        | Zero           | Zero           |
 *    | (1 byte)       | (1 byte)       | (1 byte)       | (1 byte)       |
 *    | 0x09           | 0x01-0xFF      | 0x00           | 0x00           |
 *    +----------------+----------------+----------------+----------------+
 *
 * 2. Datagram Packet Format:
 *    +----------------+----------------+------------------+
 *    | Packet Type    | Payload Length | Payload Data     |
 *    | (1 byte)       | (1 byte)       | (0-248 bytes)    |
 *    | 0x0B           | 0-248          |                  |
 *    +----------------+----------------+------------------+
 *
 * Examples:
 * 1. SYN packet:      [0x01 | 0x00 | 0x00]
 * 2. SYN-ACK packet:  [0x02 | 0x01 | 0x00]
 * 3. ACK packet:      [0x03 | 0x01 | 0x00]
 * 4. Data packet:     [0x06 | 0x04 | 0x04 | 0xDE | 0xAD | 0xBE | 0xEF]
 * 5. Data ACK:        [0x07 | 0x04 | 0x00]
 * 6. Datagram:        [0x0B | 0x03 | 0xDE | 0xAD | 0xBE]
 *
 * Note: The maximum packet size is limited by the link layer's available payload space
 * (LINK_MAX_PAYLOAD_SIZE = 250 bytes). For connection-oriented packets, the maximum
 * payload size is 246 bytes (250 - 4 header bytes). For datagrams, the maximum
 * payload size is 248 bytes (250 - 2 header bytes).
 */
#define TRANSPORT_MAX_PACKET_SIZE LINK_MAX_PAYLOAD_SIZE /**< Maximum total packet size (250 bytes) */
#define TRANSPORT_HEADER_SIZE     4                     /**< Size of transport layer header (TYPE + CONN_ID + SEQ + LENGTH) */
#define TRANSPORT_MAX_PAYLOAD_SIZE                                                                                     \
    (TRANSPORT_MAX_PACKET_SIZE - TRANSPORT_HEADER_SIZE) /**< Maximum payload size (246 bytes) */

// Transport Layer Timing Parameters
#define TRANSPORT_LAYER_DEFAULT_KEEPALIVE_MS 1000 // Default keep-alive interval (1 second)
#define TRANSPORT_LAYER_DEFAULT_TIMEOUT_MS   3000 // Default connection timeout (3 seconds)
#define TRANSPORT_ACK_TIMEOUT_MS             100  // Timeout for acknowledgment (100ms)
#define TRANSPORT_MAX_RETRIES                3    // Maximum number of retransmission attempts

/**
 * @brief Transport Layer Error Codes
 *
 * These error codes are in the transport layer range (-96 to -65)
 */
enum TransportLayerError
{
    TRANSPORT_SUCCESS = 0,
    TRANSPORT_ERROR_INVALID_PARAMS = ERROR_RANGE_TRANSPORT,
    TRANSPORT_ERROR_NOT_CONNECTED = ERROR_RANGE_TRANSPORT - 1,
    TRANSPORT_ERROR_ALREADY_CONNECTED = ERROR_RANGE_TRANSPORT - 2,
    TRANSPORT_ERROR_CONNECTION_FAILED = ERROR_RANGE_TRANSPORT - 3,
    TRANSPORT_ERROR_TIMEOUT = ERROR_RANGE_TRANSPORT - 4,
    TRANSPORT_ERROR_INVALID_PACKET = ERROR_RANGE_TRANSPORT - 5,
    TRANSPORT_ERROR_BUFFER_OVERFLOW = ERROR_RANGE_TRANSPORT - 6,
    TRANSPORT_ERROR_SEND_FAILED = ERROR_RANGE_TRANSPORT - 7,
    TRANSPORT_ERROR_INVALID_STATE = ERROR_RANGE_TRANSPORT - 8 // Layer is in an invalid state for the operation
};

/**
 * @brief Transport Layer States
 */
enum TransportState
{
    TRANSPORT_STATE_DISCONNECTED = 0,  // Initial state, not connected
    TRANSPORT_STATE_LISTENING = 1,     // Server mode: listening for connections
    TRANSPORT_STATE_CONNECTING = 2,    // Client mode: connection in progress
    TRANSPORT_STATE_CONNECTED = 3,     // Connection established
    TRANSPORT_STATE_DISCONNECTING = 4, // Graceful disconnect in progress
    TRANSPORT_STATE_ERROR = 5          // Error state
};

/**
 * @brief Transport Layer Events
 *
 * These events are reported to the upper layer to indicate state changes
 * and important events in the transport layer.
 */
enum TransportLayerEvent
{
    TRANSPORT_LAYER_EVENT_MESSAGE_RECEIVED = 0, // Message received from peer
    TRANSPORT_LAYER_EVENT_DISCONNECTED,         // Layer disconnected from peer
    TRANSPORT_LAYER_EVENT_CONNECTED,            // Successfully connected to peer
    TRANSPORT_LAYER_EVENT_INIT,                 // Layer initialized
    TRANSPORT_LAYER_EVENT_ERROR,                // Error occurred
    TRANSPORT_LAYER_EVENT_TIMEOUT,              // Connection timeout
    TRANSPORT_LAYER_EVENT_READY_FOR_DATA,       // Layer ready to handle data transmission
    TRANSPORT_LAYER_EVENT_READY_FOR_CONNECTION  // Layer ready to accept new connections
};

/**
 * @brief Transport Layer Packet Header Structure
 *
 * This structure defines the header format for transport layer packets:
 * - type: Packet type (CONNECT, DATA, ACK, etc.)
 * - connection_id: Connection identifier
 * - sequence: Sequence number for data packets
 * - length: Length of payload data
 */
struct TransportPacketHeader
{
    uint8_t type;         /**< Packet type */
    uint8_t connection_id;/**< Connection identifier */
    uint8_t sequence;     /**< Sequence number */
    uint8_t length;       /**< Payload length */
};

/**
 * @brief Transport Layer Class
 *
 * This layer provides reliable end-to-end communication with:
 * - Connection establishment and teardown
 * - Keep-alive mechanism
 * - Connection timeout detection
 * - Packet validation
 *
 * The transport layer uses the link layer for actual data transmission
 * and adds connection management on top of it.
 */
class TransportLayer : public Layer
{
public:
    // Add callback type for data reception
    typedef void (*DataCallback)(const uint8_t *data, uint16_t length);

    TransportLayer();
    virtual ~TransportLayer();

    virtual void initialize();
    virtual void deinitialize();

    // Connection management
    int connect();  // Initiates connection as client
    int listen();   // Starts listening as server
    int disconnect();
    bool is_connected() const
    {
        return state_ == TRANSPORT_STATE_CONNECTED;
    }

    // Data transmission
    virtual int send(const uint8_t *data, uint16_t length);
    virtual int send_datagram(const uint8_t *data, uint16_t length);
    virtual int on_receive(const uint8_t *data, uint16_t length);
    virtual uint16_t get_max_payload_size() const
    {
        return TRANSPORT_MAX_PAYLOAD_SIZE;
    }
    virtual void tick();

    // State management
    void set_timeout(uint32_t keepalive_ms, uint32_t timeout_ms);

    // Data callback management
    void set_data_callback(DataCallback callback)
    {
        data_callback_ = callback;
    }

private:
    // Add callback member
    DataCallback data_callback_;

    // State variables
    TransportState state_;
    uint8_t connect_retries_;
    uint32_t last_keepalive_ack_time_;
    uint8_t sequence_number_;
    uint8_t peer_sequence_number_;
    bool awaiting_ack_;
    uint32_t last_tx_time_;
    uint8_t retry_count_;
    bool waiting_response_;
    uint32_t last_tick_time_;
    uint8_t connection_id_;

    // Transport layer packet buffers (will be encapsulated as link layer payload)
    uint8_t tx_buffer_[TRANSPORT_MAX_PACKET_SIZE];      // Buffer for outgoing transport packets
    uint8_t last_tx_buffer_[TRANSPORT_MAX_PACKET_SIZE]; // Buffer for last transmitted packet
    uint16_t last_tx_length_;                           // Length of last transmitted packet

    // Timing parameters
    uint32_t keepalive_interval_;
    uint32_t connection_timeout_;
    uint32_t retry_timeout_;
    uint32_t max_retries_;

    // Internal methods for data processing
    int handle_data_packet(const uint8_t *data, uint16_t length);
    int handle_keepalive_packet(uint8_t connection_id);
    int handle_keepalive_ack_packet(uint8_t connection_id);
    int handle_datagram_packet(const uint8_t *data, uint16_t length);
    void handle_data_ack_packet(uint8_t connection_id, uint8_t sequence_number);
    void handle_data_nack_packet(uint8_t connection_id, uint8_t sequence_number);
    void send_syn();
    void send_syn_ack();
    void send_ack(uint8_t connection_id, uint8_t sequence_number);
    void send_fin();
    void send_fin_ack();
    void send_data_ack(uint8_t connection_id, uint8_t sequence_number);
    void send_data_nack(uint8_t connection_id, uint8_t sequence_number);
    void send_keepalive();

    void handle_syn_packet(uint8_t connection_id, uint8_t sequence_number);
    void handle_syn_ack_packet(uint8_t connection_id, uint8_t sequence_number);
    void handle_ack_packet(uint8_t connection_id, uint8_t sequence_number);
    void handle_fin_packet(uint8_t connection_id);
    void handle_fin_ack_packet(uint8_t connection_id);
    void reset();
};

} // namespace robust_serial
