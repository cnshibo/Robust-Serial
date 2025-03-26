#ifndef __PHYSICAL_LAYER_HPP__
#define __PHYSICAL_LAYER_HPP__

#include "layer.hpp"
#include <cstdint>

namespace robust_serial
{

/**
 * @brief Error codes specific to the Physical Layer
 * 
 * These error codes are in the physical layer error range (-32 to -1).
 * Each error code should be unique within this range.
 */
enum PhysicalLayerError {
    PHYSICAL_SUCCESS = LAYER_SUCCESS,
    PHYSICAL_ERROR_GENERAL = ERROR_RANGE_PHYSICAL,     // Generic error
    PHYSICAL_ERROR_BUSY = ERROR_RANGE_PHYSICAL - 1,    // Device is busy
    PHYSICAL_ERROR_OVERFLOW = ERROR_RANGE_PHYSICAL - 2,// Buffer overflow
    PHYSICAL_ERROR_TIMEOUT = ERROR_RANGE_PHYSICAL - 3, // Operation timeout
    PHYSICAL_ERROR_HW_FAIL = ERROR_RANGE_PHYSICAL - 4, // Hardware failure
    PHYSICAL_ERROR_INVALID_PARAM = ERROR_RANGE_PHYSICAL - 5, // Invalid parameter
    PHYSICAL_ERROR_NOT_INITIALIZED = ERROR_RANGE_PHYSICAL - 6 // Hardware not initialized
};

/**
 * @brief Physical Layer States
 */
enum PhysicalState {
    PHYSICAL_STATE_INIT = 0,    // Initial state
    PHYSICAL_STATE_READY = 1,   // Ready for operation
    PHYSICAL_STATE_BUSY = 2,    // Busy (TX or RX)
    PHYSICAL_STATE_ERROR = 3    // Error state
};

/**
 * @brief Events reported to upper layer
 */
enum PhysicalLayerEvent {
    PHYSICAL_LAYER_EVENT_READY = 0,        // Physical layer is ready for communication
    PHYSICAL_LAYER_EVENT_BUSY = 1,         // Physical layer is busy processing
    PHYSICAL_LAYER_EVENT_ERROR = -1,       // Physical layer error occurred
    PHYSICAL_LAYER_EVENT_INIT = -2,        // Physical layer initialization complete
    PHYSICAL_LAYER_EVENT_DATA_RECEIVED = -3, // Data received from physical medium
    PHYSICAL_LAYER_EVENT_DATA_SENT = -4,    // Data successfully sent to physical medium
    PHYSICAL_LAYER_EVENT_TIMEOUT = -5,      // Communication timeout occurred
    PHYSICAL_LAYER_EVENT_BUFFER_OVERFLOW = -6 // Buffer overflow occurred
};

/**
 * @brief Abstract interface for physical layer implementations
 * 
 * The Physical Layer is responsible for raw data transmission over the hardware interface.
 * It handles the lowest level of communication, dealing directly with hardware.
 * 
 * Key responsibilities:
 * - Hardware initialization and configuration
 * - Raw data transmission and reception
 * - Hardware state management
 * - Error detection and reporting
 * 
 * Note: The physical layer does not handle framing, encoding, or error correction.
 * These are handled by higher layers.
 */
class PhysicalLayer : public Layer
{
public:
    PhysicalLayer() : Layer() {
        state_ = PHYSICAL_STATE_INIT;  // Initialize state after base class construction
    }
    virtual ~PhysicalLayer() {}

    /**
     * @brief Send data through hardware interface
     * 
     * @param data Buffer to send
     * @param length Number of bytes to send
     * @return Bytes sent on success, error code on failure
     * 
     * Note: This method handles raw data transmission only.
     * The data should already be properly framed and encoded by higher layers.
     * This method is called by the upper layer when it has data to send.
     * It must return the number of bytes sent because the upper layer may need to know how many bytes were sent.   
     */
    virtual int send(const uint8_t *data, uint16_t length) = 0;

    /**
     * @brief Process received data and pass to upper layer
     * 
     * @param data Received data buffer
     * @param length Length of received data
     * @return PHYSICAL_SUCCESS on success, error code on failure
     * 
     * Note: This method handles raw data reception only.
     * The data will be processed by higher layers for framing and decoding.
     */
    virtual int on_receive(const uint8_t *data, uint16_t length) = 0;

    /**
     * @brief Get maximum payload size for this layer
     * 
     * @return Maximum payload size in bytes
     * 
     * Note: The physical layer's maximum payload size is determined by the hardware buffer size.
     * This should be implemented by concrete physical layer implementations.
     */
    virtual uint16_t get_max_payload_size() const = 0;
};

} // namespace robust_serial

#endif // __PHYSICAL_LAYER_HPP__
