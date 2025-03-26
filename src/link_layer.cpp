#include "link_layer.hpp"
#include <cstring> // For memmove()
#include "log.hpp"
#include "robust_stack.hpp"

namespace robust_serial
{

LinkLayer::LinkLayer()
    : Layer()
{
    state_ = LINK_STATE_READY;
    encoded_length_ = 0;

    outgoing_buffer_length_ = 0;
    incoming_buffer_length_ = 0;
}

/**
 * @brief Destructor for LinkLayer.
 */
LinkLayer::~LinkLayer()
{
}

/**
 * @brief Initialize the link layer
 */
void LinkLayer::initialize()
{
    // Reset state and buffers
    reset();

    // Report ready state to upper layer
    report_event(LINK_LAYER_EVENT_READY);
}

void LinkLayer::deinitialize()
{
    reset();
}

void LinkLayer::reset()
{
    state_ = LINK_STATE_READY;
    encoded_length_ = 0;
    report_event(LINK_LAYER_EVENT_READY, NULL);
}

int LinkLayer::send(const uint8_t *data, uint16_t length)
{
    if (!data || !down_layer)
    {
        report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_INVALID_PARAM;
    }

    if (length > get_max_payload_size())
    {
        report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_INVALID_PARAM;
    }

    if (state_ == LINK_STATE_ERROR)
    {
        reset(); // Auto-reset from error state on new transmission
    }

    // Construct frame: [TYPE(1) | LENGTH(1) | PAYLOAD(n) | CRC16(2)]
    frame_buffer_[0] = LINK_FRAME_TYPE_DATA;
    frame_buffer_[1] = length;
    memcpy(&frame_buffer_[LINK_HEADER_SIZE], data, length);

    uint16_t crc = CRC16::calculate(frame_buffer_, length + LINK_HEADER_SIZE);
    frame_buffer_[length + LINK_HEADER_SIZE] = crc & 0xFF;
    frame_buffer_[length + LINK_HEADER_SIZE + 1] = (crc >> 8) & 0xFF;

    uint16_t frame_length = length + LINK_MIN_FRAME_SIZE;

    // COBS encode the frame
    encoded_length_ =
        COBS::encode(frame_buffer_, frame_length, encoded_buffer_, LINK_MAX_FRAME_SIZE);
    if (encoded_length_ < 0)
    {
        state_ = LINK_STATE_ERROR;
        report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_GENERAL;
    }

    encoded_buffer_[encoded_length_] = COBS_DELIMITER;
    encoded_length_++;

    // Check if there's enough space in the outgoing buffer
    if (outgoing_buffer_length_ + encoded_length_ > LINK_OUTGOING_BUFFER_SIZE)
    {
        report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_BUFFER_FULL;
    }

    // Queue the encoded frame in the outgoing buffer
    memcpy(outgoing_buffer_ + outgoing_buffer_length_, encoded_buffer_, encoded_length_);
    outgoing_buffer_length_ += encoded_length_;

    // log_debug("LinkLayer::send, outgoing_buffer_length_:%d",
    // outgoing_buffer_length_);

    // Report that new data is available for sending
    report_event(LINK_LAYER_EVENT_OUTGOING_DATA_AVAILABLE);

    return LINK_SUCCESS;
}

int LinkLayer::process_outgoing_data()
{
    if (outgoing_buffer_length_ > 0 && state_ == LINK_STATE_READY)
    {
        state_ = LINK_STATE_SENDING;

        // Send the data
        int result = down_layer->send(outgoing_buffer_, outgoing_buffer_length_);

        // Remove sent data from the outgoing buffer
        if (result > 0)
        {
            memmove(outgoing_buffer_, outgoing_buffer_ + result, outgoing_buffer_length_ - result);
            outgoing_buffer_length_ -= result;
        }

        // Restore state even if no data was sent
        state_ = LINK_STATE_READY;

        return result;
    }
    else
    {
        return 0;
    }
}

// Always return LINK_SUCCESS
int LinkLayer::process_incoming_data()
{
    if (incoming_buffer_length_ == 0)
    {
        return LINK_SUCCESS;
    }

    while (incoming_buffer_length_ > 0)
    {
        uint16_t consumed_length = 0;
        int decoded_length = COBS::decode(incoming_buffer_, incoming_buffer_length_, decode_buffer_,
                                          LINK_MAX_FRAME_SIZE, consumed_length);

        if (decoded_length == COBS::COBS_ERROR_INCOMPLETE)
        {
            return LINK_SUCCESS; // Wait for more data
        }

        if (decoded_length < LINK_MIN_FRAME_SIZE || decoded_length < 0)
        {
            // Invalid frame, drop one byte
            memmove(incoming_buffer_, incoming_buffer_ + 1, --incoming_buffer_length_);

            // Parse rest of data
            continue;
        }

        // Decode return success; Validate payload length
        uint8_t payload_length = decode_buffer_[1];
        if (payload_length > LINK_MAX_PAYLOAD_SIZE ||
            decoded_length != (payload_length + LINK_MIN_FRAME_SIZE))
        {
            memmove(incoming_buffer_, incoming_buffer_ + consumed_length,
                    incoming_buffer_length_ - consumed_length);
            incoming_buffer_length_ -= consumed_length;

            continue;
        }

        // Payload is valid; Verify CRC
        uint16_t received_crc = (decode_buffer_[decoded_length - 2] & 0xFF) |
                                ((uint16_t)decode_buffer_[decoded_length - 1] << 8);

        uint16_t computed_crc = CRC16::calculate(decode_buffer_, decoded_length - LINK_CRC_SIZE);

        if (computed_crc == received_crc)
        {
            uint8_t frame_type = decode_buffer_[0];

            if (frame_type == LINK_FRAME_TYPE_DATA)
            {
                // Forward payload to upper layer
                if (up_layer)
                {
                    up_layer->on_receive(&decode_buffer_[LINK_HEADER_SIZE], payload_length);
                }
                state_ = LINK_STATE_READY;
                report_event(LINK_LAYER_EVENT_FRAME_RECEIVED);
            }
            else
            {
                // Unknown frame type
                state_ = LINK_STATE_ERROR;
                //report_event(LINK_LAYER_EVENT_ERROR);
            }
        }
        else
        {
            state_ = LINK_STATE_ERROR;
            report_event(LINK_LAYER_EVENT_CRC_ERROR);
        }

        // Remove processed frame
        memmove(incoming_buffer_, incoming_buffer_ + consumed_length,
                incoming_buffer_length_ - consumed_length);
        incoming_buffer_length_ -= consumed_length;
    }

    return LINK_SUCCESS;
}

int LinkLayer::on_receive(const uint8_t *data, uint16_t length)
{
    if (!data)
    {
        // report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_INVALID_PARAM;
    }

    // Check for buffer overflow
    if (incoming_buffer_length_ + length > LINK_INCOMING_BUFFER_SIZE)
    {
        // Buffer overflow - reset buffer
        incoming_buffer_length_ = 0;
        // report_event(LINK_LAYER_EVENT_ERROR);
        return LINK_ERROR_BUFFER_FULL;
    }

    // Append new data to incoming buffer
    memcpy(incoming_buffer_ + incoming_buffer_length_, data, length);
    incoming_buffer_length_ += length;

    // Report that more data is available for processing
    report_event(LINK_LAYER_EVENT_INCOMING_DATA_AVAILABLE);

    return LINK_SUCCESS;
}

} // namespace robust_serial
