#include "robust_stack.hpp"
#include "log.hpp"

namespace robust_serial
{

RobustStack::RobustStack(PhysicalLayer &phy)
    : phy_layer_(phy)
    , event_callback_(NULL)
    , data_callback_(NULL)
    , datagram_callback_(NULL)
    , state_(ROBUST_STACK_STATE_INIT)
{
}

/**
 * @brief Destructor for RobustStack.
 */
RobustStack::~RobustStack()
{
}

/**
 * @brief Initializes all layers and connects them together.
 */
void RobustStack::initialize()
{
    // Initialize all layers
    phy_layer_.initialize();
    link_layer_.initialize();
    transport_layer_.initialize();

    // Connect layers together
    link_layer_.set_down_layer(&phy_layer_);
    transport_layer_.set_down_layer(&link_layer_);

    // Set stack manager for all layers
    phy_layer_.set_stack_manager(this);
    link_layer_.set_stack_manager(this);
    transport_layer_.set_stack_manager(this);

    // Set initial state
    set_state(ROBUST_STACK_STATE_READY);
    report_event(ROBUST_STACK_EVENT_READY);
}

/**
 * @brief Resets the stack to its initial state.
 * This method can be called after a connection failure to prepare for a new connection attempt.
 */
void RobustStack::reset()
{
    // Reset all layers
    phy_layer_.deinitialize();
    link_layer_.deinitialize();
    transport_layer_.deinitialize();

    // Initialize all layers
    phy_layer_.initialize();
    link_layer_.initialize();
    transport_layer_.initialize();

    // Reset state
    set_state(ROBUST_STACK_STATE_READY);
    
    // Report ready state
    report_event(ROBUST_STACK_EVENT_READY);
    
    //log_info("RobustStack: Stack reset complete");
}

/**
 * @brief Initiates a connection.
 * @return 0 on success, error code on failure
 */
int RobustStack::connect()
{
    log_debug("RobustStack: Connect requested in state=%d", state_);
    
    // If already connected, return success
    if (state_ == ROBUST_STACK_STATE_CONNECTED)
    {
        log_debug("RobustStack: Already connected, returning success");
        return ROBUST_SUCCESS;
    }

    // If in any other state except READY, return error
    if (state_ != ROBUST_STACK_STATE_READY)
    {
        log_debug("RobustStack: Connect failed - invalid state %d", state_);
        return ROBUST_ERROR_INVALID_STATE;
    }

    // Initialize connection state
    set_state(ROBUST_STACK_STATE_CONNECTING);
    
    // Delegate connection to transport layer
    int result = transport_layer_.connect();
    if (result < 0)
    {
        set_state(ROBUST_STACK_STATE_ERROR);
        report_event(ROBUST_STACK_EVENT_ERROR);
    }

    return result;
}

/**
 * @brief Handles disconnection.
 * @return 0 on success, error code on failure
 */
int RobustStack::disconnect()
{
    log_debug("RobustStack: Disconnect requested in state=%d", state_);
    
    // If not connected, return error
    if (state_ != ROBUST_STACK_STATE_CONNECTED)
    {
        log_debug("RobustStack: Disconnect failed - not connected");
        return ROBUST_ERROR_NOT_CONNECTED;
    }

    // Delegate disconnection to transport layer
    int result = transport_layer_.disconnect();
    if (result >= 0)
    {
        set_state(ROBUST_STACK_STATE_READY);
        report_event(ROBUST_STACK_EVENT_DISCONNECTED);
    }
    else
    {
        set_state(ROBUST_STACK_STATE_ERROR);
        report_event(ROBUST_STACK_EVENT_ERROR);
    }

    return result;
}

/**
 * @brief Sends data through the stack.
 */
int RobustStack::send(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0)
    {
        return LAYER_ERROR_INVALID_PARAM;
    }

    if (state_ != ROBUST_STACK_STATE_CONNECTED)
    {
        return LAYER_ERROR_INVALID_STATE;
    }

    int result = transport_layer_.send(data, length);
    if (result >= 0)
    {
        report_event(ROBUST_STACK_EVENT_DATA_SENT);
    }
    return result;
}

/**
 * @brief Sends data directly to the transport layer.
 */
int RobustStack::send_datagram(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0)
    {
        return LAYER_ERROR_INVALID_PARAM;
    }

    if (state_ != ROBUST_STACK_STATE_READY && state_ != ROBUST_STACK_STATE_CONNECTED)
    {
        return LAYER_ERROR_INVALID_STATE;
    }

    int result = transport_layer_.send_datagram(data, length);
    if (result >= 0)
    {
        report_event(ROBUST_STACK_EVENT_DATA_SENT);
    }
    return result;
}

/**
 * @brief Processes received data from the transport layer.
 */
int RobustStack::on_receive(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0)
    {
        return LAYER_ERROR_INVALID_PARAM;
    }

    // Regular data packet
    if (data_callback_)
    {
        data_callback_(data, length);
    }
    report_event(ROBUST_STACK_EVENT_DATA_RECEIVED);

    return LAYER_SUCCESS;
}

/**
 * @brief Processes received datagram data and calls the user's datagram callback.
 * @param data Pointer to the datagram data
 * @param length Length of the datagram data
 * @return LAYER_SUCCESS on success, error code on failure
 */
int RobustStack::on_datagram(const uint8_t *data, uint16_t length)
{
    if (!data || length == 0)
    {
        return LAYER_ERROR_INVALID_PARAM;
    }

    // Call the user's datagram callback if set
    if (datagram_callback_)
    {
        datagram_callback_(data, length);
    }

    report_event(ROBUST_STACK_EVENT_DATAGRAM_RECEIVED);
    return LAYER_SUCCESS;
}

/**
 * @brief Sets timeout parameters for the transport layer.
 */
void RobustStack::set_timeout(uint32_t keepalive_ms, uint32_t timeout_ms)
{
    transport_layer_.set_timeout(keepalive_ms, timeout_ms);
}

/**
 * @brief Handles events from layers.
 */
void RobustStack::on_layer_event(Layer *source_layer, int32_t event_code, void *parameter)
{
    if (source_layer == &transport_layer_)
    {
        on_transport_layer_event(event_code, parameter);
    }
    else if (source_layer == &link_layer_)
    {
        on_link_layer_event(event_code, parameter);
    }
    else if (source_layer == &phy_layer_)
    {
        on_physical_layer_event(event_code, parameter);
    }
}

/**
 * @brief Handles events from the transport layer.
 */
void RobustStack::on_transport_layer_event(int32_t event_code, void *parameter)
{
    switch (event_code)
    {
    case static_cast<int32_t>(TRANSPORT_LAYER_EVENT_CONNECTED):
        log_info("RobustStack: connected");
        set_state(ROBUST_STACK_STATE_CONNECTED);
        report_event(ROBUST_STACK_EVENT_CONNECTED);
        break;

    case static_cast<int32_t>(TRANSPORT_LAYER_EVENT_DISCONNECTED):
        log_info("RobustStack: disconnected");
        set_state(ROBUST_STACK_STATE_READY);
        report_event(ROBUST_STACK_EVENT_DISCONNECTED);
        break;

    case static_cast<int32_t>(TRANSPORT_LAYER_EVENT_ERROR):
        log_info("RobustStack: error");
        set_state(ROBUST_STACK_STATE_ERROR);
        report_event(ROBUST_STACK_EVENT_ERROR);
        break;
    case static_cast<int32_t>(TRANSPORT_LAYER_EVENT_TIMEOUT):
        log_info("RobustStack: timeout");
        set_state(ROBUST_STACK_STATE_ERROR);
        report_event(ROBUST_STACK_EVENT_TIMEOUT);
        break;

    default:
        break;
    }
}

/**
 * @brief Handles events from the link layer.
 */
void RobustStack::on_link_layer_event(int32_t event_code, void *parameter)
{
    // Handle link layer specific events
    switch (event_code)
    {
    case static_cast<int32_t>(LINK_LAYER_EVENT_CRC_ERROR):
        log_error("RobustStack: Link Layer CRC error");
        break;

    case static_cast<int32_t>(LINK_LAYER_EVENT_FRAME_RECEIVED):
        //log_info("RobustStack: Frame received");
        break;

    case static_cast<int32_t>(LINK_ERROR_BUFFER_FULL):
        log_warning("RobustStack: Link Layer buffer overflow");
        break;

    case static_cast<int32_t>(LINK_LAYER_EVENT_OUTGOING_DATA_AVAILABLE):
        //log_info("RobustStack: Outgoing data available");
        report_event(ROBUST_STACK_EVENT_OUTGOING_DATA_AVAILABLE);
        break;

    case static_cast<int32_t>(LINK_LAYER_EVENT_INCOMING_DATA_AVAILABLE):
        //log_info("RobustStack: Incoming data available");
        report_event(ROBUST_STACK_EVENT_INCOMING_DATA_AVAILABLE);
        break;
    default:
        break;
    }
}

/**
 * @brief Handles events from the physical layer.
 */
void RobustStack::on_physical_layer_event(int32_t event_code, void *parameter)
{
    // Handle physical layer specific events
    switch (event_code)
    {
    case static_cast<int32_t>(PHYSICAL_LAYER_EVENT_ERROR):
        log_error("RobustStack: Physical Layer error");
        break;

    case static_cast<int32_t>(PHYSICAL_LAYER_EVENT_READY):
        log_info("RobustStack: Physical Layer ready");
        break;

    default:
        break;
    }
}

/**
 * @brief Updates the stack state and notifies the user.
 */
void RobustStack::set_state(RobustStackState new_state)
{
    state_ = new_state;
}

/**
 * @brief Calls the user callback if it is set.
 */
void RobustStack::report_event(RobustStackEvent event)
{
    if (event_callback_ != NULL)
    {
        event_callback_(event);
    }
}

/**
 * @brief Handles periodic tasks for all layers.
 */
void RobustStack::tick()
{
    transport_layer_.tick();
}

int RobustStack::process_outgoing_data()
{
    return link_layer_.process_outgoing_data();
}

int RobustStack::process_incoming_data()
{
    return link_layer_.process_incoming_data();
}

void RobustStack::queue_link_data(const uint8_t *data, uint16_t length)
{
    link_layer_.on_receive(data, length);
}

int RobustStack::listen()
{
    log_debug("RobustStack: Listen requested in state=%d", state_);
    
    // If already listening or connected, return success
    if (state_ == ROBUST_STACK_STATE_CONNECTING || state_ == ROBUST_STACK_STATE_CONNECTED)
    {
        log_debug("RobustStack: Already listening/connected, returning success");
        return ROBUST_SUCCESS;
    }

    // If in any other state except READY, return error
    if (state_ != ROBUST_STACK_STATE_READY)
    {
        log_debug("RobustStack: Listen failed - invalid state %d", state_);
        return ROBUST_ERROR_INVALID_STATE;
    }

    // Initialize listening state
    set_state(ROBUST_STACK_STATE_CONNECTING);
    
    // Delegate listening to transport layer
    int result = transport_layer_.listen();
    if (result < 0)
    {
        set_state(ROBUST_STACK_STATE_ERROR);
        report_event(ROBUST_STACK_EVENT_ERROR);
    }

    return result;
}

} // namespace robust_serial
