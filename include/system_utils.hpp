#ifndef __ROBUST_SERIAL_SYSTEM_UTILS_HPP_
#define __ROBUST_SERIAL_SYSTEM_UTILS_HPP_

#include <cstdint>

namespace robust_serial {

/**
 * @brief Get current system time in milliseconds
 * 
 * This is a platform-specific function that should be implemented
 * for each target platform.
 * 
 * @return Current system time in milliseconds
 */
uint32_t get_current_time_ms();

} // namespace robust_serial

#endif // __ROBUST_SERIAL_SYSTEM_UTILS_HPP_ 