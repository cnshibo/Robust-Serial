#ifndef __LAYER_HPP__
#define __LAYER_HPP__

#include <cstdint>
#include <cstddef>  // Required for NULL
#include "config.hpp"

namespace robust_serial
{

// Forward declarations
class RobustStack;

/**
 * @brief Common error codes for all layers
 * 
 * These error codes are in the base layer error range (0 to -31).
 * Each specific layer should define its own error codes in its respective range.
 */
enum LayerError {
    LAYER_ERROR = -1,              // Generic error
    LAYER_ERROR_INVALID_LAYER = -2,    // Invalid layer connection
    LAYER_ERROR_INVALID_PARAM = -3,    // Invalid parameter
    LAYER_ERROR_PAYLOAD_TOO_LARGE = -4, // Payload exceeds maximum size
    LAYER_ERROR_NOT_IMPLEMENTED = -5,   // Method not implemented by layer
    LAYER_ERROR_INVALID_STATE = -6      // Layer is in an invalid state
};

/**
 * @brief Interface for communication layers
 * 
 * Abstract interface that defines the contract for layer implementations.
 * Concrete layers must implement send() and on_receive().
 * 
 * Key concepts:
 * - Each layer is responsible for a specific aspect of communication
 * - Layers are connected in a stack (up_layer and down_layer)
 * - Data flows down through send() and up through on_receive()
 * - Events are reported to RobustStack through report_event()
 * 
 * Layer responsibilities:
 * - Physical Layer: Raw data transmission
 * - Link Layer: Frame integrity (COBS encoding, CRC)
 * - Transport Layer: Reliable delivery, flow control
 */
class Layer
{
public:
    Layer() : up_layer(NULL), down_layer(NULL), manager_(NULL), state_(0) {}
    virtual ~Layer() {}

    // Initialization
    virtual void initialize() = 0;
    virtual void deinitialize() = 0;

    /**
     * @brief Send data down through the layer stack
     * 
     * @param data Pointer to data buffer
     * @param length Length of data in bytes
     * @return Bytes processed on success, error code on failure
     */
    virtual int send(const uint8_t *data, uint16_t length) = 0;

    /**
     * @brief Process received data from lower layer
     * 
     * @param data Pointer to received data
     * @param length Length of received data
     * @return LAYER_SUCCESS on success, error code on failure
     */
    virtual int on_receive(const uint8_t *data, uint16_t length) = 0;

    /**
     * @brief Get maximum payload size for this layer
     * 
     * @return Maximum payload size in bytes
     * 
     * Note: Each layer implements its own size limit based on its requirements:
     * - Physical Layer: Hardware buffer size
     * - Link Layer: COBS encoded frame size minus headers and CRC
     * - Transport Layer: Maximum segment size
     */
    virtual uint16_t get_max_payload_size() const = 0;

    /**
     * @brief Get the current state of the layer
     * 
     * @return Current state of the layer
     * 
     * Note: Each layer implements its own state values:
     * - Physical Layer: PHYSICAL_STATE_* values
     * - Link Layer: LINK_STATE_* values
     * - Transport Layer: TRANSPORT_STATE_* values
     */
    virtual int get_state() const { return state_; }

    /**
     * @brief Check if payload size is valid for this layer
     * 
     * @param length Length to check
     * @return true if length is valid, false otherwise
     */
    bool is_valid_payload_size(uint16_t length) const {
        return length <= get_max_payload_size();
    }

    /**
     * @brief Connect to upper layer
     * 
     * @param layer Pointer to upper layer
     * @return LAYER_SUCCESS on success, error code on failure
     */
    virtual int set_up_layer(Layer *layer) {
        if (!layer) {
            return LAYER_ERROR_INVALID_LAYER;
        }
        up_layer = layer;
        layer->down_layer = this;
        return LAYER_SUCCESS;
    }

    /**
     * @brief Connect to lower layer
     * 
     * @param layer Pointer to lower layer
     * @return LAYER_SUCCESS on success, error code on failure
     */
    virtual int set_down_layer(Layer *layer) {
        if (!layer) {
            return LAYER_ERROR_INVALID_LAYER;
        }
        down_layer = layer;
        layer->up_layer = this;
        return LAYER_SUCCESS;
    }

    /**
     * @brief Set the stack manager for event reporting
     * 
     * @param manager Pointer to RobustStack instance
     */
    void set_stack_manager(RobustStack* manager) {
        manager_ = manager;
    }

    /**
     * @brief Report an event to the stack manager
     * 
     * @param event_code Event code to report
     * @param parameter Optional parameter for the event
     */
    virtual void report_event(int32_t event_code, void* parameter=NULL);

protected:
    Layer *up_layer;           // Pointer to layer above
    Layer *down_layer;         // Pointer to layer below
    RobustStack *manager_;     // Pointer to stack manager
    int state_;                // Current state of the layer
};

} // namespace robust_serial

#endif // __LAYER_HPP__
