/**
 * @file
 */

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/family.hpp"
#include "micras/hal/family/flash.hpp"
#include "micras/hal/flash.hpp"
#include "tim.h"

namespace micras::test {
TEST_SUITE("family") {
    TEST_CASE("reads the flash geometry from the board's macros") {
        CHECK(hal::family::flash_word_bits == 256);
        CHECK(hal::family::sector_size == 128 * 1024);
        CHECK(hal::family::storage_first_sector == 4);
        CHECK(hal::family::storage_sectors == 4);

        CHECK(hal::FlashWord::words == 8);
        CHECK(hal::FlashWord::size == 32);
        CHECK(hal::Flash::total_size == hal::family::storage_sectors * hal::family::sector_size);
    }

    TEST_CASE_FIXTURE(HostBoard, "takes a timer's clock from its kernel clock") {
        MX_TIM15_Init();

        CHECK(hal::family::timer_clock(htim15.Instance) == htim15.Instance->kernel_clock);
        CHECK(hal::family::timer_clock(htim15.Instance) == 200000000);
    }

    TEST_CASE("has nothing to calibrate, cache or unlock on a host") {
        ADC_HandleTypeDef adc{};

        CHECK(hal::family::calibrate_adc(&adc));
        CHECK_FALSE(hal::family::was_reset_by_watchdog());
        CHECK(hal::family::is_cpu_frequency_supported(true));
        CHECK(hal::family::is_cpu_frequency_supported(false));
        CHECK(hal::family::watchdog() != nullptr);
        CHECK(hal::family::watchdog() == hal::family::watchdog());

        hal::family::enable_caches();
        hal::family::freeze_watchdog_in_debug();
        hal::family::enable_cycle_counter();
    }

    TEST_CASE("never stops or ends a transfer on a host") {
        const DMA_HandleTypeDef dma{};

        CHECK(hal::family::is_dma_enabled(&dma));
        CHECK_FALSE(hal::family::has_dma_finished(&dma));
    }
}
}  // namespace micras::test
