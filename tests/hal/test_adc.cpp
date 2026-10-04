/**
 * @file
 */

#include <array>
#include <cstdint>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/adc_dma.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/host/ports.hpp"

namespace micras::test {
TEST_SUITE("adc") {
    TEST_CASE_FIXTURE(HostBoard, "copies a finished sequence into the snapshot") {
        hal::AdcDma             adc{adc_config};
        std::array<uint16_t, 2> buffer{};
        std::array<uint16_t, 2> snapshot{};
        std::array<uint16_t, 2> read{};

        REQUIRE(adc.was_initialized());
        REQUIRE(adc.start_dma(buffer, snapshot));

        const hal::host::AdcPort& port = hal::host::Board::adc(adc_config.handle);
        CHECK(port.size() == 2);

        port.write(0, 1500);
        port.write(1, 900);
        port.write(2, 100);
        CHECK(adc.read_snapshot(read) == 0);
        CHECK(read == std::array<uint16_t, 2>{0, 0});

        port.finish_sequence();
        CHECK(adc.read_snapshot(read) == 1);
        CHECK(read == std::array<uint16_t, 2>{1500, 900});
        CHECK(adc.get_max_reading() == 4095);
        CHECK(static_cast<double>(adc.get_reference_voltage()) == doctest::Approx(3.3));
    }

    TEST_CASE_FIXTURE(HostBoard, "stops updating after an error until it recovers") {
        hal::AdcDma             adc{adc_config};
        std::array<uint16_t, 2> buffer{};
        std::array<uint16_t, 2> snapshot{};
        std::array<uint16_t, 2> read{};
        REQUIRE(adc.start_dma(buffer, snapshot));

        const hal::host::AdcPort& port = hal::host::Board::adc(adc_config.handle);
        const uint32_t            restarts = hal::AdcDma::get_restarts();

        hal::AdcDma::on_error(adc_config.handle);
        port.write(0, 7);
        port.finish_sequence();
        CHECK(adc.read_snapshot(read) == 0);

        adc.recover();
        CHECK(hal::AdcDma::get_restarts() == restarts + 1);

        port.finish_sequence();
        CHECK(adc.read_snapshot(read) == 1);
        CHECK(read.front() == 7);
    }

    TEST_CASE_FIXTURE(HostBoard, "refuses a snapshot of another size than the buffer") {
        hal::AdcDma             adc{adc_config};
        std::array<uint16_t, 2> buffer{};
        std::array<uint16_t, 3> snapshot{};

        CHECK_FALSE(adc.start_dma(buffer, snapshot));
        CHECK_FALSE(adc.was_initialized());
    }
}
}  // namespace micras::test
