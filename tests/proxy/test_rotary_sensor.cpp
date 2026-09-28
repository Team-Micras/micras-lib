/**
 * @file
 */

#include <array>
#include <cstdint>
#include <numbers>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/crc.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/models/as5047u_model.hpp"
#include "micras/proxy/rotary_sensor.hpp"

namespace micras::test {
namespace {
const proxy::RotarySensor::Registers registers{
    .disable = {{.UVW_off = 1, .ABI_off = 0, .na = 0, .FILTER_disable = 0}},
    .zposm = {{.ZPOSM = 0}},
    .zposl = {{.ZPOSL = 0, .Dia1_en = 0, .Dia2_en = 0}},
    .settings1 = {{.K_max = 0, .K_min = 0, .Dia3_en = 0, .Dia4_en = 0}},
    .settings2 = {{
        .IWIDTH = 0,
        .NOISESET = 0,
        .DIR = 0,
        .UVW_ABI = 0,
        .DAECDIS = 0,
        .ABI_DEC = 0,
        .Data_select = 0,
        .PWMon = 0,
    }},
    .settings3 = {{.UVWPP = 0, .HYS = 0, .ABIRES = 0b100}},
    .ecc = {{.ECC_chsum = 0, .ECC_en = 0}},
};

const proxy::RotarySensor::Config rotary_sensor_config{
    .spi =
        {
            .init_function = MX_SPI3_Init,
            .handle = &hspi3,
            .cs_gpio = spi_config.cs_gpio,
            .timeout = 2,
            .clock_polarity = SPI_POLARITY_LOW,
            .clock_phase = SPI_PHASE_2EDGE,
        },
    .encoder = encoder_config,
    .crc = crc_config,
    .registers = registers,
};

void attach(const proxy::RotarySensor::Config& config, hal::host::SpiDevice& device) {
    hal::host::Board::spi_device(config.spi.handle, config.spi.cs_gpio.port, config.spi.cs_gpio.pin, device);
}
}  // namespace

TEST_SUITE("rotary_sensor") {
    TEST_CASE_FIXTURE(HostBoard, "sets the chip up and reads its resolution back") {
        models::As5047uModel chip;
        attach(rotary_sensor_config, chip);

        const proxy::RotarySensor sensor{rotary_sensor_config};

        CHECK(sensor.was_initialized());
        CHECK(sensor.get_resolution() == 16384);
        CHECK(chip.peek(models::As5047uModel::settings3_address) == registers.settings3.raw);
        CHECK(chip.peek(models::As5047uModel::disable_address) == registers.disable.raw);
        CHECK(chip.peek(models::As5047uModel::errfl_address) == 0);
    }

    TEST_CASE_FIXTURE(HostBoard, "reads the position the encoder counted") {
        models::As5047uModel chip;
        attach(rotary_sensor_config, chip);
        const proxy::RotarySensor sensor{rotary_sensor_config};

        hal::host::Board::encoder(rotary_sensor_config.encoder.handle).count = 4096;

        CHECK(sensor.get_position() == doctest::Approx(std::numbers::pi_v<float> / 2.0F));
    }

    TEST_CASE_FIXTURE(HostBoard, "fails when the chip rejects the frames for their CRC") {
        CRC_HandleTypeDef wrong_crc = hcrc;
        wrong_crc.Init.InitValue = 0x00;
        proxy::RotarySensor::Config config = rotary_sensor_config;
        config.crc.handle = &wrong_crc;
        models::As5047uModel chip;
        attach(config, chip);

        const proxy::RotarySensor sensor{config};

        CHECK_FALSE(sensor.was_initialized());
        CHECK(chip.peek(models::As5047uModel::settings3_address) == 0);
        CHECK((chip.peek(models::As5047uModel::errfl_address) & models::As5047uModel::crc_error) != 0);
    }

    TEST_CASE_FIXTURE(HostBoard, "computes the frame CRC as the CRC unit does") {
        hal::Crc crc{crc_config};

        for (const std::array<uint8_t, 2> bytes : std::vector<std::array<uint8_t, 2>>{
                 {0x00, 0x00}, {0x40, 0x01}, {0x40, 0x1A}, {0x00, 0x80}, {0x3F, 0xFF}, {0xC0, 0x15}
             }) {
            CAPTURE(bytes);
            CHECK(models::As5047uModel::crc(bytes.at(0), bytes.at(1)) == (crc.calculate(bytes) ^ 0xFFU));
        }
    }
}
}  // namespace micras::test
