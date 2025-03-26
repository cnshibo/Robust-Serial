#ifndef __COBS_HPP__
#define __COBS_HPP__

#include <cstdint>
#include "config.hpp"

namespace robust_serial
{

// COBS Protocol Configuration
static const uint8_t COBS_BLOCK_SIZE = 254;     // Maximum size of COBS block before stuffing
static const uint8_t COBS_DELIMITER = 0x00;     // COBS delimiter byte
static const uint8_t COBS_MAX_OVERHEAD = 1;     // Maximum COBS overhead per block

/**
 * @brief COBS (Consistent Overhead Byte Stuffing) encoding and decoding.
 *
 * This class provides static methods for encoding and decoding data using
 * the COBS algorithm, which is used to ensure that data frames do not
 * contain the delimiter byte (0x00).
 */
class COBS
{
public:
    /**
     * @brief Encode data using COBS.
     *
     * @param input Pointer to the input data buffer.
     * @param length Length of the input data.
     * @param output Pointer to the output buffer where encoded data will be stored.
     * @param output_size Size of the output buffer.
     * @return The number of bytes written to the output buffer, or a negative error code.
     */
    static int encode(const uint8_t* input, uint16_t length,
                     uint8_t* output, uint16_t output_size);

    /**
     * @brief Decode data using COBS.
     *
     * @param input Pointer to the encoded input data buffer.
     * @param length Length of the encoded input data.
     * @param output Pointer to the output buffer where decoded data will be stored.
     * @param output_size Size of the output buffer.
     * @param consumed_length Reference to a variable where the number of bytes consumed will be stored.
     * @return The number of bytes written to the output buffer, or a negative error code.
     */
    static int decode(const uint8_t* input, uint16_t length,
                     uint8_t* output, uint16_t output_size,
                     uint16_t& consumed_length);

    // Error codes for COBS operations
    enum CobsError {
        COBS_SUCCESS = 0,
        COBS_INVALID_PARAMETERS = -1,
        COBS_INVALID_INPUT_SIZE = -2,
        COBS_INCOMPLETE_PACKET = -3,
        COBS_OUTPUT_BUFFER_TOO_SMALL = -4,
        COBS_MALFORMED_PACKET = -5,
        COBS_ERROR_INVALID_INPUT = -6,
        COBS_ERROR_OUTPUT_TOO_SMALL = -7,
        COBS_ERROR_INCOMPLETE = -8
    };

private:
    static const uint8_t COBS_MAX_CODE = 0xFF;  // Maximum value for a COBS code byte
};

} // namespace robust_serial

#endif // __COBS_HPP__
