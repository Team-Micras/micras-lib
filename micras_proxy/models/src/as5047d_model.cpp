/**
 * @file
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "micras/hal/host/spi_device.hpp"
#include "micras/models/as5047d_model.hpp"

namespace micras::models {
namespace {
/**
 * @brief Bits of the 14-bit address or data field.
 */
constexpr uint16_t field_mask{0x3FFF};

/**
 * @brief Read flag of a command frame, and error flag of a data frame.
 */
constexpr uint16_t flag_bit{1U << 14U};

/**
 * @brief Parity bit of a frame.
 */
constexpr uint16_t parity_bit{1U << 15U};
}  // namespace

As5047dModel::As5047dModel() : SpiDevice{Mode::MODE_1}, answer{answer_with(0, false)} { }

void As5047dModel::select() {
    this->received_bytes = 0;
}

void As5047dModel::exchange(std::span<const uint8_t> transmitted, std::span<uint8_t> received) {
    const std::size_t size = std::min(transmitted.size(), received.size());
    auto              output = received.begin();

    for (const uint8_t byte : transmitted.first(size)) {
        const uint32_t shift = 8U * static_cast<uint32_t>(frame_size - 1 - this->received_bytes);
        *output = static_cast<uint8_t>(this->answer >> shift);
        output++;
        this->frame.at(this->received_bytes) = byte;
        this->received_bytes++;

        if (this->received_bytes == frame_size) {
            this->received_bytes = 0;
            this->process(this->frame);
        }
    }
}

void As5047dModel::deselect() {
    if (this->received_bytes != 0) {
        this->errfl |= framing_error;
        this->pending_write.reset();
    }

    this->received_bytes = 0;
}

uint16_t As5047dModel::peek(uint16_t address) const {
    switch (address) {
        case errfl_address:
            return this->errfl;

        case zposm_address:
            return this->zposm;

        case zposl_address:
            return this->zposl;

        case settings1_address:
            return this->settings1;

        case settings2_address:
            return this->settings2;

        default:
            return 0;
    }
}

uint16_t As5047dModel::with_parity(uint16_t frame) {
    const auto payload = static_cast<uint16_t>(frame & static_cast<uint16_t>(~parity_bit));
    return (std::popcount(payload) % 2 != 0) ? static_cast<uint16_t>(payload | parity_bit) : payload;
}

void As5047dModel::process(const std::array<uint8_t, frame_size>& frame) {
    const auto raw = static_cast<uint16_t>(std::get<0>(frame) << 8U | std::get<1>(frame));
    const auto field = static_cast<uint16_t>(raw & field_mask);

    if (std::popcount(raw) % 2 != 0) {
        this->errfl |= parity_error;
        this->pending_write.reset();
        this->answer = answer_with(0, true);
        return;
    }

    if (this->pending_write.has_value()) {
        const uint16_t address = this->pending_write.value();
        this->pending_write.reset();

        if (uint8_t* const target = this->writable(address); target != nullptr) {
            *target = static_cast<uint8_t>(field);
        }

        this->settings1 |= 0x01U;
        this->answer = answer_with(this->peek(address), false);
        return;
    }

    if (not valid_address(field)) {
        this->errfl |= invalid_command;
        this->answer = answer_with(0, true);
        return;
    }

    this->answer = answer_with(this->peek(field), false);

    if ((raw & flag_bit) == 0) {
        this->pending_write = field;
    } else if (field == errfl_address) {
        this->errfl = 0;
    }
}

uint16_t As5047dModel::answer_with(uint16_t data, bool error) {
    return with_parity(static_cast<uint16_t>((error ? flag_bit : 0U) | (data & field_mask)));
}

bool As5047dModel::valid_address(uint16_t address) {
    switch (address) {
        case nop_address:
        case errfl_address:
        case prog_address:
        case zposm_address:
        case zposl_address:
        case settings1_address:
        case settings2_address:
        case diaagc_address:
        case mag_address:
        case angleunc_address:
        case anglecom_address:
            return true;

        default:
            return false;
    }
}

uint8_t* As5047dModel::writable(uint16_t address) {
    switch (address) {
        case zposm_address:
            return &this->zposm;

        case zposl_address:
            return &this->zposl;

        case settings1_address:
            return &this->settings1;

        case settings2_address:
            return &this->settings2;

        default:
            return nullptr;
    }
}
}  // namespace micras::models
