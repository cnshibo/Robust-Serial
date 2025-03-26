#include "transport_layer.hpp"
#include "link_layer.hpp" // Ensure LinkLayer states are available
#include "robust_stack.hpp"
#include "log.hpp"
#include <cstring>

#if 0
// Disable debug logging only in this file
#undef log_debug
#define log_debug(format, ...) ((void)0)
#endif // 0

namespace robust_serial
{

TransportLayer::TransportLayer()
    : connect_retries_(0)
    , last_keepalive_ack_time_(0)
    , sequence_number_(0)
    , peer_sequence_number_(0)
    , awaiting_ack_(false)
    , last_tx_time_(0)
    , retry_count_(0)
    , waiting_response_(false)
    , keepalive_interval_(TRANSPORT_LAYER_DEFAULT_KEEPALIVE_MS)
    , connection_timeout_(TRANSPORT_LAYER_DEFAULT_TIMEOUT_MS)
    , connection_id_(TRANSPORT_CONNECTION_ID_INVALID)
{
    log_debug("TransportLayer: Constructor called");
}

/**
 * @brief Destructor for TransportLayer.
 */
TransportLayer::~TransportLayer()
{
}

/**
 * @brief Initializes or resets the Transport Layer state.
 */
void TransportLayer::initialize()
{
    // Reset all state variables
    reset();

    // Set default timing parameters
    keepalive_interval_ = TRANSPORT_LAYER_DEFAULT_KEEPALIVE_MS;
    connection_timeout_ = TRANSPORT_LAYER_DEFAULT_TIMEOUT_MS;

    log_info("TransportLayer::initialize");
}

void TransportLayer::deinitialize()
{
    reset();
}

/**
 * @brief Allows users to set custom timeout and keep-alive values.
 */
void TransportLayer::set_timeout(uint32_t keepalive_ms, uint32_t timeout_ms)
{
    log_debug("TransportLayer: Setting timeouts - keepalive=%dms, timeout=%dms", keepalive_ms,
              timeout_ms);
    keepalive_interval_ = keepalive_ms;
    connection_timeout_ = timeout_ms;
}

/**
 * @brief Initiates a connection request.
 */
int TransportLayer::connect()
{
    log_debug("TransportLayer: Connect requested in state=%d", state_);

    // If already connected, return success
    if (state_ == TRANSPORT_STATE_CONNECTED)
    {
        log_debug("TransportLayer: Already connected, returning success");
        return TRANSPORT_SUCCESS;
    }

    // If in any other state except DISCONNECTED, return error
    if (state_ != TRANSPORT_STATE_DISCONNECTED)
    {
        log_debug("TransportLayer: Connect failed - invalid state %d", state_);
        return TRANSPORT_ERROR_INVALID_STATE;
    }

    // Initialize connection state
    state_ = TRANSPORT_STATE_CONNECTING;
    connect_retries_ = 0;
    waiting_response_ = true;

    // Initialize sequence numbers with a random value to avoid predictable patterns
    sequence_number_ = (get_current_time_ms() & 0xFF); // Use lower 8 bits of timestamp
    peer_sequence_number_ = 0;                         // Will be set when we receive SYN-ACK

    log_debug("TransportLayer: Starting connection - seq=%d", sequence_number_);

    // Send initial SYN packet
    send_syn();
    return TRANSPORT_SUCCESS;
}

int TransportLayer::listen()
{
    log_debug("TransportLayer: Listen requested in state=%d", state_);

    // If already listening or connected, return success
    if (state_ == TRANSPORT_STATE_LISTENING || state_ == TRANSPORT_STATE_CONNECTED)
    {
        log_debug("TransportLayer: Already listening/connected, returning success");
        return TRANSPORT_SUCCESS;
    }

    // If in any other state except DISCONNECTED, return error
    if (state_ != TRANSPORT_STATE_DISCONNECTED)
    {
        log_debug("TransportLayer: Listen failed - invalid state %d", state_);
        return TRANSPORT_ERROR_INVALID_STATE;
    }

    // Initialize listening state
    state_ = TRANSPORT_STATE_LISTENING;
    sequence_number_ = 0; // Will be set when connection is established
    peer_sequence_number_ = 0;

    log_debug("TransportLayer: Started listening for connections");
    return TRANSPORT_SUCCESS;
}

/**
 * @brief Handles disconnection.
 */
int TransportLayer::disconnect()
{
    log_debug("TransportLayer: Disconnect requested in state=%d", state_);

    // If not connected, return error
    if (state_ != TRANSPORT_STATE_CONNECTED)
    {
        log_debug("TransportLayer: Disconnect failed - not connected");
        return TRANSPORT_ERROR_NOT_CONNECTED;
    }

    // Start graceful disconnect
    state_ = TRANSPORT_STATE_DISCONNECTING;
    waiting_response_ = true;

    log_debug("TransportLayer: Starting graceful disconnect");

    // Send FIN packet
    send_fin();
    return TRANSPORT_SUCCESS;
}

/**
 * @brief Sends data through the transport layer.
 */
int TransportLayer::send(const uint8_t *data, uint16_t length)
{
    log_debug("TransportLayer: Send requested - length=%d, state=%d", length, state_);

    if (!data || length == 0 || length > TRANSPORT_MAX_PAYLOAD_SIZE)
    {
        log_debug("TransportLayer: Send failed - invalid parameters");
        return TRANSPORT_ERROR_INVALID_PARAMS;
    }

    if (state_ != TRANSPORT_STATE_CONNECTED)
    {
        log_debug("TransportLayer: Send failed - not connected");
        return TRANSPORT_ERROR_INVALID_STATE;
    }

    // Construct transport packet directly in last_tx_buffer_: [TYPE(1) | CONN_ID(1) | SEQ(1) |
    // LENGTH(1) | PAYLOAD(n)]
    last_tx_buffer_[0] = TRANSPORT_PACKET_TYPE_DATA;
    last_tx_buffer_[1] = connection_id_;
    last_tx_buffer_[2] = sequence_number_;
    last_tx_buffer_[3] = length;
    memcpy(&last_tx_buffer_[TRANSPORT_HEADER_SIZE], data, length);
    last_tx_length_ = TRANSPORT_HEADER_SIZE + length;

    log_debug("TransportLayer: Sending data packet - seq=%d, length=%d", sequence_number_, length);

    // Send packet through link layer
    if (!down_layer)
    {
        log_debug("TransportLayer: Send failed - no down layer");
        return TRANSPORT_ERROR_INVALID_STATE;
    }

    int result = down_layer->send(last_tx_buffer_, last_tx_length_);
    if (result < 0)
    {
        log_debug("TransportLayer: Send failed - down layer error %d", result);
        return result;
    }

    waiting_response_ = true;
    last_tx_time_ = get_current_time_ms();
    sequence_number_ = (sequence_number_ + 1) % 256;

    log_debug("TransportLayer: Send successful - next seq=%d", sequence_number_);
    return TRANSPORT_SUCCESS;
}

/**
 * @brief Handles received data and reports message reception.
 */
int TransportLayer::on_receive(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0)
    {
        log_debug("TransportLayer: Received invalid packet (null or empty)");
        return -1;
    }

    if (length < sizeof(TransportPacketHeader))
    {
        //log_debug("TransportLayer: Received packet too short (length=%d, expected=%d)", length, sizeof(TransportPacketHeader));
        return -1;
    }

    const TransportPacketHeader *header = reinterpret_cast<const TransportPacketHeader *>(data);

    // Validate packet type
    if (header->type >= TRANSPORT_PACKET_TYPE_MAX)
    {
        log_debug("TransportLayer: Received invalid packet type (%d)", header->type);
        return -1;
    }

    //log_debug("TransportLayer: Received packet type=%d, conn_id=%d, seq=%d, length=%d in state=%d",
    //         header->type, header->connection_id, header->sequence, length, state_);

    // Handle different packet types based on state
    switch (header->type)
    {
    case TRANSPORT_PACKET_TYPE_SYN:
        if (state_ == TRANSPORT_STATE_LISTENING || state_ == TRANSPORT_STATE_CONNECTED)
        {
            log_debug("TransportLayer: Processing SYN packet with seq=%d", header->sequence);
            handle_syn_packet(header->connection_id, header->sequence);
            return 0;
        }
        log_debug("TransportLayer: Ignoring SYN packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_SYN_ACK:
        if (state_ == TRANSPORT_STATE_CONNECTING)
        {
            log_debug("TransportLayer: Processing SYN-ACK packet with seq=%d", header->sequence);
            handle_syn_ack_packet(header->connection_id, header->sequence);
            return 0;
        }
        log_debug("TransportLayer: Ignoring SYN-ACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_ACK:
        if (state_ == TRANSPORT_STATE_CONNECTING || state_ == TRANSPORT_STATE_DISCONNECTING)
        {
            log_debug("TransportLayer: Processing ACK packet with seq=%d", header->sequence);
            handle_ack_packet(header->connection_id, header->sequence);
            return 0;
        }
        log_debug("TransportLayer: Ignoring ACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_FIN:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            log_debug("TransportLayer: Processing FIN packet");
            handle_fin_packet(header->connection_id);
            return 0;
        }
        log_debug("TransportLayer: Ignoring FIN packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_FIN_ACK:
        if (state_ == TRANSPORT_STATE_DISCONNECTING)
        {
            log_debug("TransportLayer: Processing FIN-ACK packet");
            handle_fin_ack_packet(header->connection_id);
            return 0;
        }
        log_debug("TransportLayer: Ignoring FIN-ACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_DATA:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            log_debug("TransportLayer: Processing DATA packet with seq=%d", header->sequence);
            return handle_data_packet(data, length);
        }
        log_debug("TransportLayer: Ignoring DATA packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_DATA_ACK:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            log_debug("TransportLayer: Processing DATA_ACK packet with seq=%d", header->sequence);
            handle_data_ack_packet(header->connection_id, header->sequence);
            return 0;
        }
        log_debug("TransportLayer: Ignoring DATA_ACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_DATA_NACK:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            log_debug("TransportLayer: Processing DATA_NACK packet with seq=%d", header->sequence);
            handle_data_nack_packet(header->connection_id, header->sequence);
            return 0;
        }
        log_debug("TransportLayer: Ignoring DATA_NACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_KEEPALIVE:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            //log_debug("TransportLayer: Processing KEEPALIVE packet");
            return handle_keepalive_packet(header->connection_id);
        }
        log_debug("TransportLayer: Ignoring KEEPALIVE packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_KEEPALIVE_ACK:
        if (state_ == TRANSPORT_STATE_CONNECTED)
        {
            //log_debug("TransportLayer: Processing KEEPALIVE_ACK packet");
            return handle_keepalive_ack_packet(header->connection_id);
        }
        log_debug("TransportLayer: Ignoring KEEPALIVE_ACK packet in state=%d", state_);
        break;

    case TRANSPORT_PACKET_TYPE_DATAGRAM:
        // Datagrams can be received in any state except ERROR
        if (state_ != TRANSPORT_STATE_ERROR)
        {
            log_debug("TransportLayer: Processing DATAGRAM packet");
            return handle_datagram_packet(data, length);
        }
        log_debug("TransportLayer: Ignoring DATAGRAM packet in ERROR state");
        break;
    }

    return -1;
}

/**
 * @brief Handles data packets
 */
int TransportLayer::handle_data_packet(const uint8_t *data, uint16_t length)
{
    const TransportPacketHeader *header = reinterpret_cast<const TransportPacketHeader *>(data);

    // Verify connection ID matches
    if (header->connection_id != connection_id_)
    {
        log_debug(
            "TransportLayer: Ignoring DATA packet with invalid connection ID %d (expected %d)",
            header->connection_id, connection_id_);
        return -1;
    }

    // Extract payload
    const uint8_t *payload = data + sizeof(TransportPacketHeader);
    uint16_t payload_length = length - sizeof(TransportPacketHeader);

    log_debug("TransportLayer: Handling data packet - seq=%d, length=%d, expected_seq=%d",
              header->sequence, payload_length, peer_sequence_number_);

    // Check sequence number
    if (header->sequence != peer_sequence_number_)
    {
        log_debug("TransportLayer: Sequence mismatch - got=%d, expected=%d", header->sequence,
                  peer_sequence_number_);
        send_data_nack(connection_id_, header->sequence);
        return -1;
    }

    // Forward data directly to manager
    if (manager_)
    {
        log_debug("TransportLayer: Forwarding data to manager");
        manager_->on_receive(payload, payload_length);
    }
    else
    {
        log_debug("TransportLayer: No manager to forward data to");
    }

    // Send ACK
    send_data_ack(connection_id_, header->sequence);

    // Update peer's sequence number
    peer_sequence_number_ = (peer_sequence_number_ + 1) % 256;
    log_debug("TransportLayer: Updated peer sequence to %d", peer_sequence_number_);

    return 0;
}

/**
 * @brief Periodically checks for timeouts and sends keep-alives.
 */
void TransportLayer::tick()
{
    // Calculate elapsed time since last tick
    uint32_t current_time = get_current_time_ms();
    uint16_t elapsed_ms = static_cast<uint16_t>(current_time - last_tick_time_);
    last_tick_time_ = current_time;

    //log_debug("TransportLayer: Tick - state=%d, elapsed=%dms", state_, elapsed_ms);

    // Handle timeouts and keep-alives based on state
    switch (state_)
    {
    case TRANSPORT_STATE_CONNECTED:
        // Check for keep-alive timeout
        if (current_time - last_keepalive_ack_time_ > keepalive_interval_ * 3)
        {
            log_info("TransportLayer: Keep-alive timeout, disconnecting");
            // Start graceful disconnect
            state_ = TRANSPORT_STATE_DISCONNECTING;
            //waiting_response_ = true;
            //send_fin();
            report_event(TRANSPORT_LAYER_EVENT_TIMEOUT);
        }
        // Send keep-alive if needed
        else if (current_time - last_keepalive_ack_time_ > keepalive_interval_)
        {
            //log_debug("TransportLayer: Sending keepalive");
            send_keepalive();
        }
        break;

    case TRANSPORT_STATE_CONNECTING:
        // Handle connection timeout
        if (waiting_response_ && (current_time - last_tx_time_ > connection_timeout_))
        {
            if (connect_retries_ < TRANSPORT_MAX_RETRIES)
            {
                connect_retries_++;
                log_debug("TransportLayer: Connection timeout - retry %d/%d", connect_retries_,
                          TRANSPORT_MAX_RETRIES);
                send_syn();
            }
            else
            {
                log_debug("TransportLayer: Connection failed after %d retries", connect_retries_);
                state_ = TRANSPORT_STATE_ERROR;
                report_event(TRANSPORT_LAYER_EVENT_TIMEOUT);
            }
        }
        break;

    case TRANSPORT_STATE_DISCONNECTING:
        // Handle disconnection timeout
        if (waiting_response_ && (current_time - last_tx_time_ > connection_timeout_))
        {
            log_debug("TransportLayer: Disconnection timeout, forcing disconnect");
            state_ = TRANSPORT_STATE_DISCONNECTED;
            waiting_response_ = false;
            connection_id_ = TRANSPORT_CONNECTION_ID_INVALID;
            report_event(TRANSPORT_LAYER_EVENT_DISCONNECTED);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief Sends a keep-alive message.
 */
void TransportLayer::send_keepalive()
{
    //log_debug("TransportLayer: Sending keepalive packet - conn_id=%d", connection_id_);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_KEEPALIVE;
    tx_buffer_[1] = connection_id_;
    tx_buffer_[2] = 0; // No sequence number needed
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_syn()
{
    log_debug("TransportLayer: Sending SYN packet - seq=%d", sequence_number_);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_SYN;
    tx_buffer_[1] = TRANSPORT_CONNECTION_ID_INVALID; // Client sends invalid ID in SYN
    tx_buffer_[2] = sequence_number_;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_syn_ack()
{
    // Increment connection ID before using it
    connection_id_ = (connection_id_ + 1) % 256;
    if (connection_id_ == TRANSPORT_CONNECTION_ID_INVALID)
    {
        connection_id_ = TRANSPORT_CONNECTION_ID_START;
    }

    log_debug("TransportLayer: Sending SYN-ACK packet - seq=%d, conn_id=%d", sequence_number_,
              connection_id_);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_SYN_ACK;
    tx_buffer_[1] = connection_id_; // Use incremented connection ID
    tx_buffer_[2] = sequence_number_;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_ack(uint8_t connection_id, uint8_t sequence_number)
{
    log_debug("TransportLayer: Sending ACK packet - seq=%d, conn_id=%d", sequence_number,
              connection_id);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_ACK;
    tx_buffer_[1] = connection_id;
    tx_buffer_[2] = sequence_number;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_fin()
{
    log_debug("TransportLayer: Sending FIN packet - seq=%d, conn_id=%d", sequence_number_,
              connection_id_);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_FIN;
    tx_buffer_[1] = connection_id_;
    tx_buffer_[2] = sequence_number_;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_fin_ack()
{
    log_debug("TransportLayer: Sending FIN-ACK packet - seq=%d, conn_id=%d", sequence_number_,
              connection_id_);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_FIN_ACK;
    tx_buffer_[1] = connection_id_;
    tx_buffer_[2] = sequence_number_;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_data_ack(uint8_t connection_id, uint8_t sequence_number)
{
    log_debug("TransportLayer: Sending DATA_ACK packet - seq=%d, conn_id=%d", sequence_number,
              connection_id);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_DATA_ACK;
    tx_buffer_[1] = connection_id;
    tx_buffer_[2] = sequence_number;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::send_data_nack(uint8_t connection_id, uint8_t sequence_number)
{
    log_debug("TransportLayer: Sending DATA_NACK packet - seq=%d, conn_id=%d", sequence_number,
              connection_id);
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_DATA_NACK;
    tx_buffer_[1] = connection_id;
    tx_buffer_[2] = sequence_number;
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
}

void TransportLayer::handle_syn_packet(uint8_t connection_id, uint8_t sequence_number)
{
    // Store peer's sequence number
    peer_sequence_number_ = sequence_number;

    // Handle client reset scenario in CONNECTED state
    if (state_ == TRANSPORT_STATE_CONNECTED && connection_id == TRANSPORT_CONNECTION_ID_INVALID)
    {
        log_info("TransportLayer: Client reset detected, disconnecting current connection");
        // Send FIN to current connection
        //send_fin();
        // Transition to DISCONNECTING state to wait for FIN-ACK
        state_ = TRANSPORT_STATE_DISCONNECTED;
        //waiting_response_ = true;
        //report 
        report_event(TRANSPORT_LAYER_EVENT_ERROR);
        
        return;
    }

    // Only accept SYN packets in LISTENING state
    if (state_ != TRANSPORT_STATE_LISTENING)
    {
        log_debug("TransportLayer: Ignoring SYN packet in state %d", state_);
        return;
    }

    // Verify the connection ID is invalid (0) as expected from client
    if (connection_id != TRANSPORT_CONNECTION_ID_INVALID)
    {
        log_debug("TransportLayer: Rejecting SYN with invalid connection ID %d (expected %d)",
                  connection_id, TRANSPORT_CONNECTION_ID_INVALID);
        return;
    }

    // Accept connection while listening
    state_ = TRANSPORT_STATE_CONNECTING;
    waiting_response_ = true;
    sequence_number_ = (get_current_time_ms() & 0xFF);
    log_info("TransportLayer: Accepting connection while listening");

    // Send SYN-ACK with our allocated connection ID
    send_syn_ack();
}

void TransportLayer::handle_syn_ack_packet(uint8_t connection_id, uint8_t sequence_number)
{
    // Only handle SYN-ACK in CONNECTING state
    if (state_ != TRANSPORT_STATE_CONNECTING)
    {
        log_info("TransportLayer: Ignoring SYN-ACK packet in state %d", state_);
        return;
    }

    // Store the connection ID assigned by the server
    connection_id_ = connection_id;

    // Update peer's sequence number
    peer_sequence_number_ = sequence_number;

    // Send ACK to complete three-way handshake
    send_ack(connection_id, sequence_number);

    // Now we can safely transition to CONNECTED
    state_ = TRANSPORT_STATE_CONNECTED;
    waiting_response_ = false;
    connect_retries_ = 0;
    last_keepalive_ack_time_ = get_current_time_ms();  // Initialize keep-alive time when connected
    log_info("TransportLayer: Connection established with ID %d", connection_id_);
    report_event(TRANSPORT_LAYER_EVENT_CONNECTED);
}

void TransportLayer::handle_ack_packet(uint8_t connection_id, uint8_t sequence_number)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring ACK with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return;
    }

    // Handle ACK in CONNECTING state (completing three-way handshake)
    if (state_ == TRANSPORT_STATE_CONNECTING)
    {
        // Verify this is the ACK we're waiting for
        if (sequence_number == sequence_number_)
        {
            state_ = TRANSPORT_STATE_CONNECTED;
            waiting_response_ = false;
            connect_retries_ = 0;
            last_keepalive_ack_time_ = get_current_time_ms();  // Initialize keep-alive time when connected
            log_info("TransportLayer: Connection established with ID %d", connection_id_);
            report_event(TRANSPORT_LAYER_EVENT_CONNECTED);
        }
    }
    // Handle ACK in DISCONNECTING state (completing graceful disconnect)
    else if (state_ == TRANSPORT_STATE_DISCONNECTING)
    {
        state_ = TRANSPORT_STATE_DISCONNECTED;
        waiting_response_ = false;
        connection_id_ = TRANSPORT_CONNECTION_ID_INVALID;
        log_info("TransportLayer: Disconnection completed");
        report_event(TRANSPORT_LAYER_EVENT_DISCONNECTED);
    }
}

void TransportLayer::handle_fin_packet(uint8_t connection_id)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring FIN with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return;
    }

    // Only handle FIN in CONNECTED state
    if (state_ != TRANSPORT_STATE_CONNECTED)
    {
        return;
    }

    // Send ACK for FIN
    send_ack(connection_id, sequence_number_);

    // Send our own FIN
    send_fin();

    // Update state
    state_ = TRANSPORT_STATE_DISCONNECTING;
    waiting_response_ = true;
}

void TransportLayer::handle_fin_ack_packet(uint8_t connection_id)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring FIN-ACK with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return;
    }

    // Only handle FIN-ACK in DISCONNECTING state
    if (state_ != TRANSPORT_STATE_DISCONNECTING)
    {
        return;
    }

    // Update state
    state_ = TRANSPORT_STATE_DISCONNECTED;
    waiting_response_ = false;
    log_info("TransportLayer: Disconnection completed");
    report_event(TRANSPORT_LAYER_EVENT_DISCONNECTED);
}

void TransportLayer::handle_data_ack_packet(uint8_t connection_id, uint8_t sequence_number)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring DATA_ACK with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return;
    }

    // Check if we're waiting for this ACK
    if (!awaiting_ack_ || sequence_number != sequence_number_ - 1)
    {
        return;
    }

    // Update state
    awaiting_ack_ = false;
    retry_count_ = 0;
}

void TransportLayer::handle_data_nack_packet(uint8_t connection_id, uint8_t sequence_number)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring DATA_NACK with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return;
    }

    // Check if we're waiting for this sequence number
    if (!awaiting_ack_ || sequence_number != sequence_number_ - 1)
    {
        return;
    }

    // Resend the packet using last_tx_buffer_
    if (down_layer)
    {
        down_layer->send(last_tx_buffer_, last_tx_length_);
    }
}

int TransportLayer::handle_keepalive_packet(uint8_t connection_id)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug("TransportLayer: Ignoring KEEPALIVE with invalid connection ID %d (expected %d)",
                  connection_id, connection_id_);
        return -1;
    }

    // Send keep-alive ACK
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_KEEPALIVE_ACK;
    tx_buffer_[1] = connection_id_;
    tx_buffer_[2] = 0; // No sequence number needed
    tx_buffer_[3] = 0; // No payload
    down_layer->send(tx_buffer_, TRANSPORT_HEADER_SIZE);
    return 0;
}

int TransportLayer::handle_keepalive_ack_packet(uint8_t connection_id)
{
    // Verify connection ID matches
    if (connection_id != connection_id_)
    {
        log_debug(
            "TransportLayer: Ignoring KEEPALIVE_ACK with invalid connection ID %d (expected %d)",
            connection_id, connection_id_);
        return -1;
    }

    //log_debug("TransportLayer:handle_keepalive_ack_packet");
    // Update last received time only when we get KEEPALIVE_ACK
    last_keepalive_ack_time_ = get_current_time_ms();
    return 0;
}

void TransportLayer::reset()
{
    state_ = TRANSPORT_STATE_DISCONNECTED;
    connect_retries_ = 0;
    last_keepalive_ack_time_ = 0;
    sequence_number_ = 0;
    peer_sequence_number_ = 0;
    awaiting_ack_ = false;
    last_tx_time_ = 0;
    retry_count_ = 0;
    waiting_response_ = false;
    last_tick_time_ = get_current_time_ms();
}

int TransportLayer::send_datagram(const uint8_t *data, uint16_t length)
{
    log_debug("TransportLayer: Send datagram requested - length=%d", length);

    if (!data || !down_layer)
    {
        log_debug("TransportLayer: Send datagram failed - invalid parameters");
        return TRANSPORT_ERROR_INVALID_PARAMS;
    }

    if (length > TRANSPORT_MAX_PAYLOAD_SIZE)
    {
        log_debug("TransportLayer: Send datagram failed - payload too large (%d > %d)", length,
                  TRANSPORT_MAX_PAYLOAD_SIZE);
        return TRANSPORT_ERROR_INVALID_PARAMS;
    }

    // Construct datagram packet: [TYPE(1) | LENGTH(1) | DATA(n)]
    tx_buffer_[0] = TRANSPORT_PACKET_TYPE_DATAGRAM;
    tx_buffer_[1] = length;
    memcpy(&tx_buffer_[2], data, length);

    // Send through link layer
    int result = down_layer->send(tx_buffer_, length + 2);
    if (result < 0)
    {
        log_debug("TransportLayer: Send datagram failed - down layer error %d", result);
        return TRANSPORT_ERROR_SEND_FAILED;
    }

    log_debug("TransportLayer: Datagram sent successfully");
    return result;
}

/**
 * @brief Handles datagram packets
 */
int TransportLayer::handle_datagram_packet(const uint8_t *data, uint16_t length)
{
    // Extract payload (skip TYPE and LENGTH bytes)
    const uint8_t *payload = data + 2;
    uint16_t payload_length = length - 2;

    // Forward data directly to manager
    if (manager_)
    {
        manager_->on_datagram(payload, payload_length);
    }

    return 0;
}

} // namespace robust_serial
