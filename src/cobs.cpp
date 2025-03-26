#include "cobs.hpp"

namespace robust_serial
{
/**
 * @brief Encodes data using Consistent Overhead Byte Stuffing (COBS).
 * 
 * COBS encoding ensures that no zero bytes appear in the encoded data, making it
 * suitable for protocols that use zero bytes as delimiters.
 * 
 * @param input Pointer to input data buffer
 * @param input_length Length of input data in bytes
 * @param output Pointer to output buffer for encoded data
 * @param output_length Length of output buffer in bytes
 * @return int Number of bytes written to output buffer, or negative error code:
 *         COBS_ERROR_INVALID_PARAMS: Invalid input parameters
 *         COBS_ERROR_INPUT_TOO_LARGE: Input too large
 *         COBS_ERROR_OUTPUT_OVERFLOW: Output buffer too small
 */
int COBS::encode(const uint8_t *input, uint16_t input_length, 
                 uint8_t *output, uint16_t output_size)
{
    if (!input || !output)
    {
        return COBS_ERROR_INVALID_INPUT;
    }

    if (input_length == 0)
    {
        return 0;
    }

    // Check if input length exceeds COBS block size
    if (input_length > COBS_BLOCK_SIZE)
    {
        return COBS_ERROR_INVALID_INPUT;
    }

    // Check if output buffer is large enough
    // Maximum COBS overhead is 1 byte per 254 bytes of data, plus 1 byte
    uint16_t max_encoded_length = input_length + (input_length / 254) + 1;
    if (output_size < max_encoded_length)
    {
        return COBS_ERROR_OUTPUT_TOO_SMALL;
    }

    uint16_t read_index = 0;
    uint16_t write_index = 1;  // Leave space for first code byte
    uint16_t code_index = 0;   // Where to write the code (run length)
    uint8_t code = 1;          // Run length counter

    while (read_index < input_length)
    {
        if (input[read_index] == 0)
        {
            output[code_index] = code;
            code = 1;
            code_index = write_index++;
        }
        else
        {
            output[write_index++] = input[read_index];
            code++;
            if (code == COBS_MAX_CODE)
            {
                output[code_index] = code;
                code = 1;
                code_index = write_index++;
            }
        }
        read_index++;
    }

    output[code_index] = code;

    return write_index;
}

/**
 * @brief Decodes COBS-encoded data with improved error handling.
 * 
 * Decodes data that was encoded using the COBS algorithm, restoring the original
 * data including zero bytes.
 * 
 * @param input Pointer to input encoded data buffer
 * @param input_length Length of input encoded data in bytes
 * @param output Pointer to output buffer for decoded data
 * @param output_length Length of output buffer in bytes
 * @param consumed_length Reference to store number of input bytes consumed
 * @return int Number of bytes written to output buffer, or negative error code:
 *         COBS_ERROR_INVALID_PARAMS: Invalid input parameters
 *         COBS_ERROR_MALFORMED: Invalid zero in COBS encoding
 *         COBS_ERROR_INCOMPLETE: More data needed
 *         COBS_ERROR_OUTPUT_OVERFLOW: Output buffer too small
 */
int COBS::decode(const uint8_t *input, uint16_t input_length,
                 uint8_t *output, uint16_t output_size,
                 uint16_t &consumed_length)
{
    if (!input || !output)
    {
        return COBS_ERROR_INVALID_INPUT;
    }

    if (input_length == 0)
    {
        consumed_length = 0;
        return 0;
    }

    // Look for frame delimiter
    uint16_t frame_end;
    for (frame_end = 0; frame_end < input_length; frame_end++)
    {
        if (input[frame_end] == COBS_DELIMITER)
        {
            break;
        }
    }

    if (frame_end == input_length)
    {
        // No delimiter found
        return COBS_ERROR_INCOMPLETE;
    }

    if (frame_end == 0)
    {
        // Empty frame
        consumed_length = 1;
        return 0;
    }

    // Check if output buffer is large enough
    // Decoded data is always smaller than encoded
    if (output_size < frame_end)
    {
        return COBS_ERROR_OUTPUT_TOO_SMALL;
    }

    uint16_t read_index = 0;
    uint16_t write_index = 0;
    
    while (read_index < frame_end)
    {
        uint8_t code = input[read_index];
        if (code == 0)
        {
            return COBS_ERROR_INVALID_INPUT;  // Zero is invalid as code
        }

        read_index++;

        // Check if we have enough bytes in input
        if (read_index + code - 1 > frame_end)
        {
            return COBS_ERROR_INVALID_INPUT;
        }

        // Copy non-zero bytes
        for (uint8_t i = 1; i < code; i++)
        {
            output[write_index++] = input[read_index++];
        }

        // Add zero unless it's the last code
        if (code < COBS_MAX_CODE && read_index < frame_end)
        {
            output[write_index++] = 0;
        }
    }

    consumed_length = frame_end + 1;  // Include delimiter
    return write_index;
}

} // namespace robust_serial
