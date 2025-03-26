#ifndef __ROBUST_STACK_HPP__
#define __ROBUST_STACK_HPP__

#include "layer.hpp"
#include "physical_layer.hpp"
#include "link_layer.hpp"
#include "transport_layer.hpp"

namespace robust_serial
{

// Forward declarations
class Layer;

/**
 * @brief User-defined callback type for state and event notifications.
 */
typedef void (*RobustStackEventCallback)(int32_t event);

/**
 * @brief User-defined callback type for regular data reception.
 */
typedef void (*RobustStackDataCallback)(const uint8_t *data, uint16_t length);

/**
 * @brief User-defined callback type for datagram reception.
 */
typedef void (*RobustStackDatagramCallback)(const uint8_t *data, uint16_t length);

/**
 * @brief States for the RobustStack
 */
enum RobustStackState
{
    ROBUST_STACK_STATE_INIT = 0,   // Initial state
    ROBUST_STACK_STATE_READY = 1,  // Stack is ready for operation
    ROBUST_STACK_STATE_CONNECTING, // Connection in progress
    ROBUST_STACK_STATE_CONNECTED,  // Connected and ready for data
    ROBUST_STACK_STATE_ERROR       // Error state
};

/**
 * @brief Error codes for RobustStack
 */
enum RobustStackError
{
    ROBUST_SUCCESS = 0,
    ROBUST_ERROR_INVALID_STATE = -1,
    ROBUST_ERROR_NOT_CONNECTED = -2,
    ROBUST_ERROR_INVALID_PARAM = -3,
    ROBUST_ERROR_TIMEOUT = -4,
    ROBUST_ERROR_BUFFER_FULL = -5
};

/**
 * @brief Events reported to user
 */
enum RobustStackEvent
{
    ROBUST_STACK_EVENT_READY = 0,        // Stack is ready for operation
    ROBUST_STACK_EVENT_CONNECTED,        // Successfully connected
    ROBUST_STACK_EVENT_DISCONNECTED,     // Disconnected from peer
    ROBUST_STACK_EVENT_ERROR,            // Error occurred
    ROBUST_STACK_EVENT_TIMEOUT,          // Connection timeout
    ROBUST_STACK_EVENT_DATA_RECEIVED,    // Data received from peer
    ROBUST_STACK_EVENT_DATA_SENT,        // Data successfully sent
    ROBUST_STACK_EVENT_DATAGRAM_RECEIVED, // Datagram received from peer
    ROBUST_STACK_EVENT_OUTGOING_DATA_AVAILABLE, // Outgoing data is available
    ROBUST_STACK_EVENT_INCOMING_DATA_AVAILABLE  // Incoming data is available
};

/**
 * @brief Stack manager class that coordinates all layers
 *
 * This class manages the communication stack and coordinates between layers:
 * - Physical Layer: Hardware communication
 * - Link Layer: Frame integrity
 * - Transport Layer: Reliable delivery
 */
class RobustStack
{
public:
    RobustStack(PhysicalLayer &phy);
    ~RobustStack();

    // Initialization
    void initialize();

    // Connection management
    int connect();  // Initiates connection as client
    int listen();   // Starts listening as server
    int disconnect();
    bool is_connected() const
    {
        return state_ == ROBUST_STACK_STATE_CONNECTED;
    }

    // Data transmission
    int send(const uint8_t *data, uint16_t length);
    int send_datagram(const uint8_t *data, uint16_t length);
    int on_receive(const uint8_t *data, uint16_t length);
    int on_datagram(const uint8_t *data, uint16_t length);

    // Event handling
    void on_layer_event(Layer *source_layer, int32_t event_code, void *parameter);

    void set_event_callback(RobustStackEventCallback callback)
    {
        event_callback_ = callback;
    }
    void set_data_callback(RobustStackDataCallback callback)
    {
        data_callback_ = callback;
    }
    void set_datagram_callback(RobustStackDatagramCallback callback)
    {
        datagram_callback_ = callback;
    }

    // Configuration
    void set_timeout(uint32_t keepalive_ms, uint32_t timeout_ms);
    int get_state() const
    {
        return state_;
    }

    // Periodic updates
    void tick();

    // Reset functionality
    void reset();

    int process_outgoing_data();
    int process_incoming_data();

    void queue_link_data(const uint8_t *data, uint16_t length);

private:
    // Layer instances
    TransportLayer transport_layer_;
    LinkLayer link_layer_;
    PhysicalLayer &phy_layer_;

    // State management
    RobustStackState state_;
    RobustStackEventCallback event_callback_;
    RobustStackDataCallback data_callback_;
    RobustStackDatagramCallback datagram_callback_;

    // Layer event handlers
    void on_physical_layer_event(int32_t event_code, void *parameter);
    void on_link_layer_event(int32_t event_code, void *parameter);
    void on_transport_layer_event(int32_t event_code, void *parameter);

    // Internal methods
    void report_event(RobustStackEvent event);
    void set_state(RobustStackState new_state);

    // Prevent copy and assignment
    RobustStack(const RobustStack &);
    RobustStack &operator=(const RobustStack &);
};

} // namespace robust_serial

#endif // __ROBUST_STACK_HPP__
