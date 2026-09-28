/**
 * @file
 */

#include <cstdint>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/host/clock.hpp"
#include "micras/hal/timer.hpp"

namespace micras::test {
TEST_SUITE("clock") {
    TEST_CASE_FIXTURE(HostBoard, "charges a quantum per read and hands over at every step") {
        int steps = 0;
        hal::host::Clock::instance().set_handover(125, [&steps] { steps++; });

        const uint32_t start = hal::Timer::get_counter();

        while (hal::Timer::to_microseconds(hal::Timer::get_counter() - start) < 1000) { }

        CHECK(steps == 8);
        CHECK(hal::host::Clock::instance().steps() == 8);
        CHECK(hal::Timer::get_counter_ms() == 1);
    }

    TEST_CASE_FIXTURE(HostBoard, "counts cycles at the core clock and converts them") {
        const uint32_t cycles_per_microsecond = SystemCoreClock / 1000000;

        CHECK(hal::Timer::to_cycles(10) == 10 * cycles_per_microsecond);
        CHECK(hal::Timer::to_microseconds(10 * cycles_per_microsecond) == 10);

        const uint32_t first = hal::Timer::get_counter();
        const uint32_t second = hal::Timer::get_counter();
        CHECK(second - first == cycles_per_microsecond);
    }

    TEST_CASE_FIXTURE(HostBoard, "stops handing over once cleared") {
        int steps = 0;
        hal::host::Clock::instance().set_handover(1, [&steps] { steps++; });
        hal::Timer::get_counter();
        hal::host::Clock::instance().clear_handover();
        wait_us(10);

        CHECK(steps == 1);
    }
}
}  // namespace micras::test
