/**
 * @file
 */

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <variant>

#include "micras/proxy/rotary_sensor.hpp"
#include "micras/proxy/stopwatch.hpp"

namespace micras::proxy {
RotarySensor::RotarySensor(const Config& config) :
    spi{config.spi},
    encoder{config.encoder},
    crc{config.crc},
    chip{std::holds_alternative<As5047dRegisters>(config.registers) ? Chip::AS5047D : Chip::AS5047U} {
    if (not this->spi.was_initialized() or not this->encoder.was_initialized()) {
        return;
    }

    std::optional<uint32_t> read_back;

    if (const auto* const registers = std::get_if<Registers>(&config.registers); registers != nullptr) {
        read_back = this->configure(*registers);
    } else if (
        const auto* const as5047d_registers = std::get_if<As5047dRegisters>(&config.registers);
        as5047d_registers != nullptr
    ) {
        read_back = this->configure(*as5047d_registers);
    }

    if (not read_back.has_value()) {
        return;
    }

    this->resolution = read_back.value();
    this->initialized = true;
}

float RotarySensor::get_position() const {
    if (this->resolution == 0) {
        return 0.0F;
    }

    return static_cast<float>(this->encoder.get_counter()) * 2.0F * std::numbers::pi_v<float> /
           static_cast<float>(this->resolution);
}

uint32_t RotarySensor::get_resolution() const {
    return this->resolution;
}

bool RotarySensor::was_initialized() const {
    return this->initialized;
}

std::array<uint8_t, RotarySensor::frame_size> RotarySensor::serialize(uint32_t raw) {
    std::array<uint8_t, frame_size> bytes{
        static_cast<uint8_t>(raw >> 16),
        static_cast<uint8_t>(raw >> 8),
        static_cast<uint8_t>(raw),
    };

    std::get<2>(bytes) = static_cast<uint8_t>(this->crc.calculate({bytes.data(), frame_size - 1}) ^ 0xFF);

    return bytes;
}

std::array<uint8_t, RotarySensor::parity_frame_size> RotarySensor::serialize_with_parity(uint16_t raw) {
    const auto payload = static_cast<uint16_t>(raw & 0x7FFFU);
    const auto frame = static_cast<uint16_t>(payload | (static_cast<uint16_t>(std::popcount(payload) & 1) << 15U));

    return {static_cast<uint8_t>(frame >> 8U), static_cast<uint8_t>(frame)};
}

template <std::size_t size>
std::optional<uint32_t> RotarySensor::exchange_frame(const std::array<uint8_t, size>& frame) {
    std::array<uint8_t, size> received{};

    if (not this->spi.select_device()) {
        return std::nullopt;
    }

    const bool transferred = this->spi.transmit_receive(frame, received);
    this->spi.unselect_device();
    proxy::Stopwatch::sleep_us(min_deselect_time_us);

    if (not transferred) {
        return std::nullopt;
    }

    uint32_t value{};

    for (const uint8_t byte : received) {
        value = value << 8U | byte;
    }

    return value;
}

std::optional<uint16_t> RotarySensor::read_register(uint16_t address) {
    if (this->chip == Chip::AS5047D) {
        return this->read_parity_register(address);
    }

    const CommandFrame                    command{{.crc = 0, .address = address, .rw = 1, .do_not_care = 0}};
    const std::array<uint8_t, frame_size> command_bytes = this->serialize(command.raw);

    if (not this->exchange_frame(command_bytes).has_value()) {
        return std::nullopt;
    }

    const std::optional<uint32_t> answer = this->exchange_frame(command_bytes);

    if (not answer.has_value()) {
        return std::nullopt;
    }

    DataFrame data{};
    data.raw = answer.value();

    const std::array<uint8_t, frame_size> expected = this->serialize(answer.value());

    if (std::get<2>(expected) != data.fields.crc or data.fields.error != 0) {
        return std::nullopt;
    }

    return static_cast<uint16_t>(data.fields.data);
}

bool RotarySensor::write_register(uint16_t address, uint16_t data) {
    if (this->chip == Chip::AS5047D) {
        return this->write_parity_register(address, data);
    }

    const CommandFrame command{{.crc = 0, .address = address, .rw = 0, .do_not_care = 0}};
    const DataFrame    value{{.crc = 0, .data = data, .error = 0, .warning = 0}};

    return this->exchange_frame(this->serialize(command.raw)).has_value() and
           this->exchange_frame(this->serialize(value.raw)).has_value();
}

std::optional<uint16_t> RotarySensor::read_parity_register(uint16_t address) {
    const ParityCommandFrame                     command{{.address = address, .rw = 1, .parity = 0}};
    const std::array<uint8_t, parity_frame_size> command_bytes = serialize_with_parity(command.raw);

    if (not this->exchange_frame(command_bytes).has_value()) {
        return std::nullopt;
    }

    const std::optional<uint32_t> answer = this->exchange_frame(command_bytes);

    if (not answer.has_value()) {
        return std::nullopt;
    }

    ParityDataFrame data{};
    data.raw = static_cast<uint16_t>(answer.value());

    if (std::popcount(data.raw) % 2 != 0 or data.fields.error != 0) {
        return std::nullopt;
    }

    return static_cast<uint16_t>(data.fields.data);
}

bool RotarySensor::write_parity_register(uint16_t address, uint16_t data) {
    const ParityCommandFrame command{{.address = address, .rw = 0, .parity = 0}};
    const ParityDataFrame    value{{.data = data, .error = 0, .parity = 0}};

    return this->exchange_frame(serialize_with_parity(command.raw)).has_value() and
           this->exchange_frame(serialize_with_parity(value.raw)).has_value();
}

std::optional<uint32_t> RotarySensor::configure(const Registers& registers) {
    this->read_register(Registers::errfl_addr);

    this->write_register(Registers::disable_addr, registers.disable.raw);
    this->write_register(Registers::zposm_addr, registers.zposm.raw);
    this->write_register(Registers::zposl_addr, registers.zposl.raw);
    this->write_register(Registers::settings1_addr, registers.settings1.raw);
    this->write_register(Registers::settings2_addr, registers.settings2.raw);
    this->write_register(Registers::settings3_addr, registers.settings3.raw);
    this->write_register(Registers::ecc_addr, registers.ecc.raw);

    return this->read_resolution();
}

std::optional<uint32_t> RotarySensor::configure(const As5047dRegisters& registers) {
    this->read_register(As5047dRegisters::errfl_addr);

    this->write_register(As5047dRegisters::zposm_addr, registers.zposm.raw);
    this->write_register(As5047dRegisters::zposl_addr, registers.zposl.raw);
    this->write_register(As5047dRegisters::settings1_addr, registers.settings1.raw);
    this->write_register(As5047dRegisters::settings2_addr, registers.settings2.raw);

    return this->read_as5047d_resolution();
}

std::optional<uint32_t> RotarySensor::read_resolution() {
    const std::optional<uint16_t> settings3 = this->read_register(Registers::settings3_addr);

    if (not settings3.has_value()) {
        return std::nullopt;
    }

    Registers::Settings3 fields{};
    fields.raw = static_cast<uint8_t>(settings3.value());

    const uint16_t pulses = pulses_per_revolution.at(fields.fields.ABIRES);

    if (pulses == 0) {
        return std::nullopt;
    }

    return edges_per_pulse * pulses;
}

std::optional<uint32_t> RotarySensor::read_as5047d_resolution() {
    const std::optional<uint16_t> settings1 = this->read_register(As5047dRegisters::settings1_addr);
    const std::optional<uint16_t> settings2 = this->read_register(As5047dRegisters::settings2_addr);

    if (not settings1.has_value() or not settings2.has_value()) {
        return std::nullopt;
    }

    As5047dRegisters::Settings1 settings1_fields{};
    settings1_fields.raw = static_cast<uint8_t>(settings1.value());
    As5047dRegisters::Settings2 settings2_fields{};
    settings2_fields.raw = static_cast<uint8_t>(settings2.value());

    const std::array<uint16_t, 8>& table = settings1_fields.fields.ABIBIN != 0 ? as5047d_binary_pulses_per_revolution :
                                                                                 as5047d_decimal_pulses_per_revolution;
    const uint16_t                 pulses = table.at(settings2_fields.fields.ABIRES);

    if (pulses == 0) {
        return std::nullopt;
    }

    return edges_per_pulse * pulses;
}
}  // namespace micras::proxy
