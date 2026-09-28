/**
 * @file
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/host/clock.hpp"
#include "micras/hal/host/ports.hpp"
#include "micras/hal/host/spi_device.hpp"
#include "micras/hal/spi.hpp"
#include "micras/hal/timer.hpp"

namespace micras::test {
namespace {
using hal::host::Board;
using hal::host::SpiDevice;

class RecordingDevice : public SpiDevice {
public:
    RecordingDevice(Mode mode, uint8_t offset) : SpiDevice{mode}, offset{offset} { }

    void select() override { this->log.emplace_back("select"); }

    void exchange(std::span<const uint8_t> transmitted, std::span<uint8_t> received) override {
        this->log.push_back("exchange " + std::to_string(transmitted.size()));

        this->received_bytes.insert(this->received_bytes.end(), transmitted.begin(), transmitted.end());
        std::ranges::transform(transmitted, received.begin(), [this](uint8_t byte) {
            return static_cast<uint8_t>(byte + this->offset);
        });
    }

    void deselect() override { this->log.emplace_back("deselect"); }

    const std::vector<std::string>& events() const { return this->log; }

    const std::vector<uint8_t>& bytes() const { return this->received_bytes; }

private:
    uint8_t                  offset;
    std::vector<std::string> log;
    std::vector<uint8_t>     received_bytes;
};
}  // namespace

static void attach(const hal::Spi::Config& config, SpiDevice& device) {
    Board::spi_device(config.handle, config.cs_gpio.port, config.cs_gpio.pin, device);
}

static uint32_t wait_for_transfer(const hal::Spi& spi) {
    const uint64_t start = hal::host::Clock::instance().now();

    while (spi.get_transfer() == hal::Spi::Transfer::RUNNING) {
        hal::Timer::get_counter();
    }

    return static_cast<uint32_t>((hal::host::Clock::instance().now() - start) / (SystemCoreClock / 1000000));
}

TEST_SUITE("spi") {
    TEST_CASE_FIXTURE(HostBoard, "routes each transfer to the device its chip select selects") {
        RecordingDevice first_chip{SpiDevice::Mode::MODE_3, 0x10};
        RecordingDevice second_chip{SpiDevice::Mode::MODE_1, 0x20};
        attach(spi_config, first_chip);
        attach(second_spi_config, second_chip);
        hal::Spi first{spi_config};
        hal::Spi second{second_spi_config};

        const std::array<uint8_t, 1> command{0x8F};
        const std::array<uint8_t, 2> frame{0x40, 0x01};
        std::array<uint8_t, 2>       answer{};

        REQUIRE(first.select_device());
        CHECK(first.transmit(command));
        first.unselect_device();
        REQUIRE(second.select_device());
        CHECK(second.transmit_receive(frame, answer));
        second.unselect_device();

        CHECK(first_chip.bytes() == std::vector<uint8_t>{0x8F});
        CHECK(second_chip.bytes() == std::vector<uint8_t>{0x40, 0x01});
        CHECK(answer == std::array<uint8_t, 2>{0x60, 0x21});
        CHECK(Board::unbound().empty());
    }

    TEST_CASE_FIXTURE(HostBoard, "makes one transaction of the calls between select and unselect") {
        RecordingDevice chip{SpiDevice::Mode::MODE_3, 1};
        attach(spi_config, chip);
        hal::Spi spi{spi_config};

        const std::array<uint8_t, 1> command{0x8F};
        std::array<uint8_t, 2>       data{};

        REQUIRE(spi.select_device());
        CHECK(spi.transmit(command));
        CHECK(spi.receive(data));
        spi.unselect_device();

        CHECK(chip.events() == std::vector<std::string>{"select", "exchange 1", "exchange 2", "deselect"});
        CHECK(data == std::array<uint8_t, 2>{1, 1});
    }

    TEST_CASE_FIXTURE(HostBoard, "reaches no device whose chip select is high") {
        RecordingDevice chip{SpiDevice::Mode::MODE_3, 1};
        attach(spi_config, chip);
        hal::Spi spi{spi_config};

        std::array<uint8_t, 2> data{};
        CHECK(spi.receive(data));

        CHECK(chip.events().empty());
        CHECK(data == std::array<uint8_t, 2>{0xFF, 0xFF});
    }

    TEST_CASE_FIXTURE(HostBoard, "answers all ones where no device is attached") {
        hal::Spi spi{spi_config};

        std::array<uint8_t, 2> data{};
        REQUIRE(spi.select_device());
        CHECK(spi.receive(data));
        spi.unselect_device();

        CHECK(data == std::array<uint8_t, 2>{0xFF, 0xFF});
        CHECK(Board::unbound() == std::vector<std::string>{"SPI_CSn", "hspi3 SPI_CSn"});
    }

    TEST_CASE_FIXTURE(HostBoard, "answers all ones in a mode the device does not answer in") {
        RecordingDevice chip{SpiDevice::Mode::MODE_0, 1};
        attach(spi_config, chip);
        hal::Spi spi{spi_config};

        const std::array<uint8_t, 2> transmitted{0x8F, 0x00};
        std::array<uint8_t, 2>       received{};
        REQUIRE(spi.select_device());
        CHECK(spi.transmit_receive(transmitted, received));
        spi.unselect_device();

        CHECK(chip.events() == std::vector<std::string>{"select", "deselect"});
        CHECK(received == std::array<uint8_t, 2>{0xFF, 0xFF});
    }

    TEST_CASE_FIXTURE(HostBoard, "keeps the device selected while a transfer runs") {
        RecordingDevice chip{SpiDevice::Mode::MODE_3, 1};
        attach(spi_config, chip);
        hal::Spi spi{spi_config};

        std::array<uint8_t, 17>    transmitted{};
        std::array<uint8_t, 17>    received{};
        const hal::host::GpioPort& chip_select = Board::gpio(spi_config.cs_gpio.port, spi_config.cs_gpio.pin);

        REQUIRE(spi.start_transfer(transmitted, received));

        CHECK(spi.get_transfer() == hal::Spi::Transfer::RUNNING);
        CHECK_FALSE(chip_select.output);
        CHECK(chip.events() == std::vector<std::string>{"select", "exchange 17"});
    }

    TEST_CASE_FIXTURE(HostBoard, "completes a transfer when its last bit is out") {
        RecordingDevice chip{SpiDevice::Mode::MODE_3, 1};
        attach(spi_config, chip);
        hal::Spi spi{spi_config};

        std::array<uint8_t, 17>    transmitted{};
        std::array<uint8_t, 17>    received{};
        const hal::host::GpioPort& chip_select = Board::gpio(spi_config.cs_gpio.port, spi_config.cs_gpio.pin);

        REQUIRE(spi.start_transfer(transmitted, received));
        const uint32_t elapsed = wait_for_transfer(spi);

        CHECK(spi.get_transfer() == hal::Spi::Transfer::COMPLETE);
        CHECK(chip_select.output);
        CHECK(chip.events() == std::vector<std::string>{"select", "exchange 17", "deselect"});
        CHECK(received.back() == 1);
        CHECK(elapsed >= 35);
        CHECK(elapsed <= 36);
    }

    TEST_CASE_FIXTURE(HostBoard, "waits for the bus before selecting another device") {
        RecordingDevice first_chip{SpiDevice::Mode::MODE_3, 1};
        RecordingDevice second_chip{SpiDevice::Mode::MODE_1, 1};
        attach(spi_config, first_chip);
        attach(second_spi_config, second_chip);
        hal::Spi first{spi_config};
        hal::Spi second{second_spi_config};

        std::array<uint8_t, 17> transmitted{};
        std::array<uint8_t, 17> received{};
        const uint64_t          start = hal::host::Clock::instance().now();

        REQUIRE(first.start_transfer(transmitted, received));
        REQUIRE(second.select_device());
        const uint64_t waited = hal::host::Clock::instance().now() - start;
        second.unselect_device();

        CHECK(first.get_transfer() == hal::Spi::Transfer::COMPLETE);
        CHECK(first_chip.events().back() == "deselect");
        CHECK(waited >= 35U * (SystemCoreClock / 1000000));
    }
}
}  // namespace micras::test
