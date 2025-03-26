#ifndef __CRC16_HPP__
#define __CRC16_HPP__

#include <cstdint>

namespace robust_serial
{

/**
 * @brief CRC16 calculation class
 * 
 * This class provides CRC16 calculation functionality using a lookup table
 * for improved performance. The CRC16 implementation uses the standard
 * polynomial 0x1021 (CRC-16-CCITT).
 */
class CRC16
{
public:
    /**
     * @brief Calculate CRC16 for a data buffer
     * 
     * @param data Buffer to calculate CRC for
     * @param length Length of data buffer
     * @return Calculated CRC16 value
     */
    static uint16_t calculate(const uint8_t *data, uint16_t length);

    // CRC Configuration
    static const uint16_t CRC_POLYNOMIAL = 0x1021;    // CRC-16-CCITT polynomial
    static const uint16_t CRC_INITIAL_VALUE = 0xFFFF; // Initial CRC value

private:
    static const uint16_t CRC16_TABLE[256];  // Lookup table for CRC16 calculation
};

} // namespace robust_serial

#endif // __CRC16_HPP__
