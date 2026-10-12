/**
 * @file
 */

#include <array>
#include <bit>
#include <cstdint>

#include <doctest/doctest.h>

#include "micras/hal/host/spi_device.hpp"
#include "micras/models/as5047d_model.hpp"

namespace micras::models {
namespace {
using Frame = std::array<uint8_t, As5047dModel::frame_size>;

constexpr uint16_t read_flag{1U << 14U};
constexpr uint16_t error_flag{1U << 14U};
}  // namespace

static Frame frame_of(uint16_t frame) {
    const uint16_t with_parity = As5047dModel::with_parity(frame);
    return {static_cast<uint8_t>(with_parity >> 8U), static_cast<uint8_t>(with_parity)};
}

static Frame read_command(uint16_t address) {
    return frame_of(static_cast<uint16_t>(read_flag | address));
}

static Frame write_command(uint16_t address) {
    return frame_of(address);
}

static Frame data_frame(uint16_t data) {
    return frame_of(data);
}

static uint16_t transfer(As5047dModel& chip, const Frame& frame) {
    Frame received{};
    chip.select();
    chip.exchange(frame, received);
    chip.deselect();
    return static_cast<uint16_t>(std::get<0>(received) << 8U | std::get<1>(received));
}

static uint16_t data_of(uint16_t answer) {
    return static_cast<uint16_t>(answer & 0x3FFFU);
}

static bool valid(uint16_t answer) {
    return std::popcount(answer) % 2 == 0;
}

static uint16_t read_register(As5047dModel& chip, uint16_t address) {
    transfer(chip, read_command(address));
    return data_of(transfer(chip, read_command(As5047dModel::nop_address)));
}

static void write_register(As5047dModel& chip, uint16_t address, uint16_t data) {
    transfer(chip, write_command(address));
    transfer(chip, data_frame(data));
}

TEST_SUITE("as5047d_model") {
    TEST_CASE("answers in SPI mode 1") {
        const As5047dModel chip;

        CHECK(chip.mode() == hal::host::SpiDevice::Mode::MODE_1);
    }

    TEST_CASE("sets the parity bit to make the ones even") {
        CHECK(As5047dModel::with_parity(0x0000) == 0x0000);
        CHECK(As5047dModel::with_parity(0x4001) == 0x4001);
        CHECK(As5047dModel::with_parity(0x4018) == 0xC018);
        CHECK(As5047dModel::with_parity(0x7FFF) == 0xFFFF);
        CHECK(As5047dModel::with_parity(0x8000) == 0x0000);
    }

    TEST_CASE("powers on with the datasheet's defaults") {
        As5047dModel chip;

        CHECK(chip.peek(As5047dModel::settings1_address) == 0x01);
        CHECK(chip.peek(As5047dModel::settings2_address) == 0);
        CHECK(chip.peek(As5047dModel::errfl_address) == 0);
        CHECK(transfer(chip, read_command(As5047dModel::nop_address)) == 0x0000);
    }

    TEST_CASE("answers each frame in the next one") {
        As5047dModel chip;
        write_register(chip, As5047dModel::settings2_address, 0x50);
        write_register(chip, As5047dModel::zposm_address, 0x13);

        transfer(chip, read_command(As5047dModel::settings2_address));
        const uint16_t settings2 = transfer(chip, read_command(As5047dModel::zposm_address));
        const uint16_t zposm = transfer(chip, read_command(As5047dModel::nop_address));

        CHECK(data_of(settings2) == 0x50);
        CHECK(data_of(zposm) == 0x13);
        CHECK(valid(settings2));
        CHECK(valid(zposm));
    }

    TEST_CASE("keeps the factory bit of SETTINGS1 set") {
        As5047dModel chip;

        write_register(chip, As5047dModel::settings1_address, 0x20);

        CHECK(read_register(chip, As5047dModel::settings1_address) == 0x21);
    }

    TEST_CASE("rejects a frame with a wrong parity") {
        As5047dModel chip;
        Frame        command = write_command(As5047dModel::settings2_address);
        std::get<0>(command) ^= 0x80;

        transfer(chip, command);
        const uint16_t after_error = transfer(chip, data_frame(0x20));

        CHECK(chip.peek(As5047dModel::settings2_address) == 0);
        CHECK((chip.peek(As5047dModel::errfl_address) & As5047dModel::parity_error) != 0);
        CHECK((after_error & error_flag) != 0U);
        CHECK(valid(after_error));
    }

    TEST_CASE("flags a command to an address it does not have") {
        As5047dModel chip;

        transfer(chip, read_command(0x001A));
        const uint16_t after_error = transfer(chip, read_command(As5047dModel::errfl_address));
        const uint16_t errfl = transfer(chip, read_command(As5047dModel::nop_address));

        CHECK((after_error & error_flag) != 0U);
        CHECK(data_of(errfl) == As5047dModel::invalid_command);
        CHECK(chip.peek(As5047dModel::errfl_address) == 0);
    }

    TEST_CASE("flags a frame cut short by the chip select") {
        As5047dModel                 chip;
        const Frame                  command = write_command(As5047dModel::settings2_address);
        const std::array<uint8_t, 1> partial{std::get<0>(command)};
        std::array<uint8_t, 1>       received{};

        chip.select();
        chip.exchange(partial, received);
        chip.deselect();
        transfer(chip, data_frame(0x20));

        CHECK(chip.peek(As5047dModel::settings2_address) == 0);
        CHECK((chip.peek(As5047dModel::errfl_address) & As5047dModel::framing_error) != 0);
    }
}
}  // namespace micras::models
