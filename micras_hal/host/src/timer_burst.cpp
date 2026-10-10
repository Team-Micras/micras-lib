/**
 * @file
 */

#include <cstdint>
#include <span>

#include "micras/hal/host/board.hpp"
#include "micras/hal/host/ports.hpp"
#include "micras/hal/timer_burst.hpp"

namespace micras::hal {
TimerBurst::TimerBurst(const Config& config) : handle{config.handle} {
    if (this->handle->State == HAL_TIM_STATE_RESET) {
        config.init_function();
    }

    this->initialized = true;
}

bool TimerBurst::arm(
    std::span<const uint32_t> first, std::span<const uint32_t> second, std::span<const uint32_t> table
) {
    if (not this->initialized or first.empty() or second.size() != first.size() or table.size() % first.size() != 0) {
        return false;
    }

    host::TimerBurstPort& port = host::Board::timer_burst(this->handle);
    port.touched = true;
    port.first = first;
    port.second = second;
    port.table = table;
    port.running = false;
    port.arms++;
    return true;
}

void TimerBurst::start() {
    host::Board::timer_burst(this->handle).running = true;
}

void TimerBurst::stop() {
    host::Board::timer_burst(this->handle).running = false;
}

bool TimerBurst::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::hal
