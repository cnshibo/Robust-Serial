#include "layer.hpp"
#include "robust_stack.hpp"

namespace robust_serial
{

void Layer::report_event(int32_t event_code, void* parameter)
{
    if (manager_) {
        manager_->on_layer_event(this, event_code, parameter);
    }
}

} // namespace robust_serial 