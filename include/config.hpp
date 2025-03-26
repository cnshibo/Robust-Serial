#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <cstdint>

namespace robust_serial
{

/**
 * @brief Configuration parameters for the robust serial implementation.
 * 
 * This file contains global configuration constants for the robust serial
 * protocol, including buffer sizes and error code ranges.
 */

// Common Layer Error Codes
static const int32_t LAYER_SUCCESS = 0;

/**
 * @brief Maximum size of raw data that can be sent in a single frame.
 * 
 * This value is set to 254 bytes to match the COBS block size limit.
 * COBS encoding requires that no block of data between zero bytes
 * exceeds 254 bytes. This ensures that the COBS code byte (which
 * indicates the distance to the next zero byte) never exceeds 255.
 * 
 * Note: This is the maximum size of the actual data payload. The
 * total frame size will be larger due to headers, CRC, and COBS encoding.
 */
static const uint16_t COBS_MAX_BLOCK_SIZE = 254;    // Maximum size of raw data before COBS encoding

/**
 * @brief Maximum size of a complete frame after COBS encoding.
 * 
 * This value is calculated as:
 * - COBS_MAX_BLOCK_SIZE (254 bytes)
 * - Plus COBS overhead (1 byte per 254 bytes + 1 byte)
 * - Plus frame delimiter (1 byte)
 * 
 * Total: 254 + (254/254) + 1 + 1 = 257 bytes
 * 
 * This is the maximum size that any frame can have after COBS encoding,
 * including all overhead bytes. This value is critical for buffer sizing
 * in both the physical and link layers.
 */
static const uint16_t COBS_MAX_ENCODED_SIZE = 257;      // Maximum size after COBS encoding (including delimiter)

// Error Code Ranges
// Each layer gets a range of 32 error codes
static const int32_t ERROR_RANGE_PHYSICAL = -32;  // Physical layer errors: -32 to -1
static const int32_t ERROR_RANGE_LINK = -64;      // Link layer errors: -64 to -33
static const int32_t ERROR_RANGE_TRANSPORT = -96; // Transport layer errors: -96 to -65
static const int32_t ERROR_RANGE_APPLICATION = -128; // Application layer errors: -128 to -97

} // namespace robust_serial

#endif // __CONFIG_HPP__ 