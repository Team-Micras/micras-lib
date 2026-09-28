/**
 * @file
 */

#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/gpio.hpp"
#include "micras/hal/host/board.hpp"

namespace micras::test {
TEST_SUITE("board") {
    TEST_CASE_FIXTURE(HostBoard, "reads what the outside drives and otherwise what the firmware wrote") {
        hal::Gpio led{status_led_config};
        led.write(true);

        hal::host::GpioPort& port = hal::host::Board::gpio(status_led_config.port, status_led_config.pin);
        CHECK(port.output);
        CHECK(led.read());

        port.input = false;
        CHECK_FALSE(led.read());
        CHECK(port.touched);

        led.toggle();
        CHECK_FALSE(port.output);
    }

    TEST_CASE_FIXTURE(HostBoard, "names a port after the label of its pin") {
        const hal::Gpio button{button_config};
        static_cast<void>(button.read());

        CHECK(hal::host::Board::gpio(button_config.port, button_config.pin).name == "Button");
    }

    TEST_CASE_FIXTURE(HostBoard, "reports the ports the firmware touched that nothing is bound to") {
        hal::Gpio led{status_led_config};
        led.write(true);

        CHECK(hal::host::Board::unbound() == std::vector<std::string>{"Status_LED"});

        hal::host::Board::gpio(status_led_config.port, status_led_config.pin).bound = true;
        CHECK(hal::host::Board::unbound().empty());
    }

    TEST_CASE_FIXTURE(HostBoard, "forgets every port on a reset") {
        hal::Gpio led{status_led_config};
        led.write(true);
        hal::host::Board::reset();

        CHECK(hal::host::Board::unbound().empty());
        CHECK_FALSE(hal::host::Board::gpio(status_led_config.port, status_led_config.pin).output);
    }
}
}  // namespace micras::test
