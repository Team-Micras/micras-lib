/**
 * @file
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "micras/hal/host/ports.hpp"

namespace micras::hal::host {
void AdcPort::write(std::size_t index, uint32_t counts) const {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) bounded just above
    if (index < this->buffer16.size()) {
        this->buffer16[index] = static_cast<uint16_t>(counts);
    } else if (index < this->buffer32.size()) {
        this->buffer32[index] = counts;
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}

void AdcPort::finish_sequence() const {
    if (this->complete) {
        this->complete();
    }
}

void UartPort::receive(uint8_t byte) {
    if (this->rx_buffer.empty()) {
        return;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) the head wraps below the size
    this->rx_buffer[this->rx_head] = byte;
    this->rx_head = (this->rx_head + 1) % this->rx_buffer.size();
}

void FmacPort::configure(
    std::span<const int16_t> feed_forward_coefficients, std::span<const int16_t> feedback_coefficients
) {
    this->feed_forward.assign(feed_forward_coefficients.begin(), feed_forward_coefficients.end());
    this->feedback.assign(feedback_coefficients.begin(), feedback_coefficients.end());
    this->inputs.assign(this->feed_forward.size(), 0);
    this->outputs.assign(this->feedback.size(), 0);
}

int16_t FmacPort::filter(int16_t sample) {
    if (this->feed_forward.empty()) {
        return 0;
    }

    this->inputs.pop_back();
    this->inputs.push_front(sample);

    int64_t accumulator = 0;

    for (std::size_t tap = 0; tap < this->feed_forward.size(); tap++) {
        accumulator += static_cast<int64_t>(this->feed_forward.at(tap)) * this->inputs.at(tap);
    }

    for (std::size_t tap = 0; tap < this->feedback.size(); tap++) {
        accumulator += static_cast<int64_t>(this->feedback.at(tap)) * this->outputs.at(tap);
    }

    constexpr int64_t fractional_bits{15};
    const auto        output = static_cast<int16_t>(std::clamp<int64_t>(
        accumulator >> fractional_bits, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max()
    ));

    if (not this->outputs.empty()) {
        this->outputs.pop_back();
        this->outputs.push_front(output);
    }

    return output;
}
}  // namespace micras::hal::host
