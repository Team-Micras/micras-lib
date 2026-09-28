/**
 * @file
 */

#include <array>
#include <cstdint>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/gpio.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/mcu.hpp"
#include "micras/hal/pwm.hpp"

extern "C" {
void SystemClock_Config();
void PeriphCommonClock_Config();
}

namespace micras::test {
TEST_SUITE("mcu") {
    TEST_CASE_FIXTURE(HostBoard, "runs the clock and peripheral init functions it is given") {
        constexpr std::array<hal::Mcu::InitFunction, 2> inits{MX_TIM15_Init, MX_UART4_Init};

        hal::Mcu::init({
            .clock_init = SystemClock_Config,
            .peripheral_clock_init = PeriphCommonClock_Config,
            .peripheral_inits = inits,
            .cpu_frequency_boost = false,
        });

        CHECK(htim15.State == HAL_TIM_STATE_READY);
        CHECK(huart4.gState == HAL_UART_STATE_READY);
        CHECK(hal::Mcu::is_cpu_frequency_supported());
        CHECK_FALSE(hal::Mcu::was_reset_by_watchdog());
    }

    TEST_CASE_FIXTURE(HostBoard, "counts a watchdog expiry when a refresh comes late") {
        hal::Mcu::set_watchdog_timeout(10);
        wait_us(5000);
        hal::Mcu::refresh_watchdog();
        CHECK(hal::host::Board::mcu().watchdog_expiries == 0);

        wait_us(11000);
        hal::Mcu::refresh_watchdog();
        CHECK(hal::host::Board::mcu().watchdog_expiries == 1);
    }

    TEST_CASE_FIXTURE(HostBoard, "stops the outputs in an emergency") {
        hal::Pwm  pwm{pwm_config};
        hal::Gpio enable{status_led_config};
        pwm.set_duty_cycle(80.0F);
        enable.write(true);

        const std::array<hal::Pwm::Config, 1>  outputs{pwm_config};
        const std::array<hal::Gpio::Config, 1> enables{status_led_config};
        hal::Mcu::emergency_stop(outputs, enables);

        CHECK(hal::host::Board::pwm(pwm_config.handle, pwm_config.timer_channel).duty_cycle == 0.0F);
        CHECK_FALSE(hal::host::Board::gpio(status_led_config.port, status_led_config.pin).output);
        CHECK(hal::host::Board::mcu().emergency_stops == 1);
    }
}
}  // namespace micras::test
