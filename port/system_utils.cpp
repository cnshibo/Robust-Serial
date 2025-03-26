#include "system_utils.hpp"
#include "FreeRTOS.h"
#include "task.h"

namespace robust_serial {

uint32_t get_current_time_ms()
{
    uint32_t time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return time_ms;
}

} // namespace robust_serial 