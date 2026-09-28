/**
 * @file
 */

#include <cstdint>
#include <span>

#include "micras/hal/fmac.hpp"
#include "micras/hal/host/board.hpp"

namespace micras::hal {
Fmac::Fmac(const Config& config) : handle{config.handle} {
    if (this->handle->State == HAL_FMAC_STATE_RESET) {
        config.init_function();
    }
}

bool Fmac::configure_iir(std::span<const int16_t> feed_forward, std::span<const int16_t> feedback) {
    this->initialized = false;

    if (feed_forward.empty() or feedback.empty() or feed_forward.size() + feedback.size() > buffer_size or
        this->handle->State != HAL_FMAC_STATE_READY) {
        return false;
    }

    host::FmacPort& port = host::Board::fmac(this->handle);
    port.touched = true;
    port.configure(feed_forward, feedback);

    this->initialized = true;
    return true;
}

int16_t Fmac::update(int16_t sample) {
    return host::Board::fmac(this->handle).filter(sample);
}

bool Fmac::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::hal
