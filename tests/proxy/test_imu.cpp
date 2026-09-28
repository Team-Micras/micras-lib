/**
 * @file
 */

#include <cstdint>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/host/clock.hpp"
#include "micras/hal/host/spi_device.hpp"
#include "micras/models/lsm6dsv_model.hpp"
#include "micras/proxy/imu.hpp"

namespace micras::test {
namespace {
const proxy::Imu::Config imu_config{
    .spi = spi_config,
    .gyroscope_mode = LSM6DSV_GY_HIGH_ACCURACY_ODR_MD,
    .accelerometer_mode = LSM6DSV_XL_HIGH_ACCURACY_ODR_MD,
    .gyroscope_data_rate = LSM6DSV_ODR_HA01_AT_8000Hz,
    .accelerometer_data_rate = LSM6DSV_ODR_HA01_AT_8000Hz,
    .gyroscope_scale = LSM6DSV_4000dps,
    .accelerometer_scale = LSM6DSV_8g,
    .gyroscope_filter = LSM6DSV_GY_ULTRA_LIGHT,
    .accelerometer_filter = LSM6DSV_XL_MEDIUM,
};
}  // namespace

static void attach(const proxy::Imu::Config& config, hal::host::SpiDevice& device) {
    hal::host::Board::spi_device(config.spi.handle, config.spi.cs_gpio.port, config.spi.cs_gpio.pin, device);
}

static bool near(float value, double expected, double tolerance) {
    const auto actual = static_cast<double>(value);
    return actual >= expected - tolerance and actual <= expected + tolerance;
}

TEST_SUITE("imu") {
    TEST_CASE_FIXTURE(HostBoard, "sets the chip up as its configuration says") {
        models::Lsm6dsvModel chip;
        attach(imu_config, chip);

        const proxy::Imu imu{imu_config};

        CHECK(imu.was_initialized());
        CHECK(chip.gyroscope_data_rate() == 8000.0);
        CHECK(chip.accelerometer_data_rate() == 8000.0);
        CHECK((chip.peek(LSM6DSV_CTRL6) & 0x0FU) == LSM6DSV_4000dps);
        CHECK((chip.peek(LSM6DSV_CTRL8) & 0x03U) == LSM6DSV_8g);
        CHECK((chip.peek(LSM6DSV_CTRL3) & 0x40U) == 0x40);
        CHECK((chip.peek(LSM6DSV_CTRL7) & 0x01U) == 0x01);
        CHECK((chip.peek(LSM6DSV_CTRL9) & 0x08U) == 0x08);
    }

    TEST_CASE_FIXTURE(HostBoard, "waits for the chip to start before configuring it") {
        models::Lsm6dsvModel chip;
        attach(imu_config, chip);
        const uint64_t start = hal::host::Clock::instance().now();

        const proxy::Imu imu{imu_config};
        const uint64_t   elapsed_us = (hal::host::Clock::instance().now() - start) / (SystemCoreClock / 1000000);

        CHECK(imu.was_initialized());
        CHECK(elapsed_us >= 39900);
        CHECK(elapsed_us < 41000);
    }

    TEST_CASE_FIXTURE(HostBoard, "reads a sample one update after the update that asked for it") {
        models::Lsm6dsvModel chip;
        attach(imu_config, chip);
        proxy::Imu imu{imu_config};

        chip.push_sample({0.1, -0.2, 3.0}, {1.0, -2.0, 9.80665});
        imu.update();
        CHECK_FALSE(imu.is_new());

        wait_us(125);
        imu.update();

        REQUIRE(imu.is_new());
        const double gyroscope = chip.gyroscope_sensitivity();
        const double accelerometer = chip.accelerometer_sensitivity();
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::X), 0.1, gyroscope));
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::Y), -0.2, gyroscope));
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::Z), 3.0, gyroscope));
        CHECK(near(imu.get_linear_acceleration(proxy::Imu::Axis::X), 1.0, accelerometer));
        CHECK(near(imu.get_linear_acceleration(proxy::Imu::Axis::Y), -2.0, accelerometer));
        CHECK(near(imu.get_linear_acceleration(proxy::Imu::Axis::Z), 9.80665, accelerometer));

        wait_us(125);
        imu.update();
        CHECK_FALSE(imu.is_new());
    }

    TEST_CASE_FIXTURE(HostBoard, "reads the same motion at another full scale") {
        proxy::Imu::Config config = imu_config;
        config.gyroscope_scale = LSM6DSV_250dps;
        config.accelerometer_scale = LSM6DSV_2g;
        models::Lsm6dsvModel chip;
        attach(config, chip);
        proxy::Imu imu{config};

        chip.push_sample({0.5, 0.0, -1.5}, {0.0, 3.0, 9.80665});
        imu.update();
        wait_us(125);
        imu.update();

        REQUIRE(imu.is_new());
        const double gyroscope = chip.gyroscope_sensitivity();
        const double accelerometer = chip.accelerometer_sensitivity();
        CHECK((chip.peek(LSM6DSV_CTRL6) & 0x0FU) == LSM6DSV_250dps);
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::X), 0.5, gyroscope));
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::Z), -1.5, gyroscope));
        CHECK(near(imu.get_linear_acceleration(proxy::Imu::Axis::Y), 3.0, accelerometer));
        CHECK(near(imu.get_linear_acceleration(proxy::Imu::Axis::Z), 9.80665, accelerometer));
    }

    TEST_CASE_FIXTURE(HostBoard, "saturates a motion beyond the full scale it was set to") {
        proxy::Imu::Config config = imu_config;
        config.gyroscope_scale = LSM6DSV_250dps;
        models::Lsm6dsvModel chip;
        attach(config, chip);
        proxy::Imu imu{config};

        chip.push_sample({10.0, 0.0, 0.0}, {0.0, 0.0, 9.80665});
        imu.update();
        wait_us(125);
        imu.update();

        REQUIRE(imu.is_new());
        CHECK(near(imu.get_angular_velocity(proxy::Imu::Axis::X), 32767 * chip.gyroscope_sensitivity(), 1e-3));
        CHECK(imu.get_angular_velocity(proxy::Imu::Axis::X) < 5.1F);
    }

    TEST_CASE_FIXTURE(HostBoard, "fails without its chip") {
        const proxy::Imu imu{imu_config};

        CHECK_FALSE(imu.was_initialized());
    }

    TEST_CASE_FIXTURE(HostBoard, "fails in the wrong SPI mode") {
        proxy::Imu::Config config = imu_config;
        config.spi.clock_polarity = SPI_POLARITY_LOW;
        models::Lsm6dsvModel chip;
        attach(config, chip);

        const proxy::Imu imu{config};

        CHECK_FALSE(imu.was_initialized());
    }
}
}  // namespace micras::test
