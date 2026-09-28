/**
 * @file
 */

#include <array>
#include <cstdint>
#include <deque>
#include <span>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/uart_dma.hpp"

namespace micras::test {
TEST_SUITE("uart") {
    TEST_CASE_FIXTURE(HostBoard, "reads what arrived through the circular receive buffer") {
        hal::UartDma           uart{uart_config};
        std::array<uint8_t, 8> ring{};
        REQUIRE(uart.start_rx(ring));
        CHECK(uart.was_initialized());

        hal::host::UartPort&   port = hal::host::Board::uart(uart_config.handle);
        std::array<uint8_t, 8> received{};

        for (uint8_t byte = 1; byte <= 6; byte++) {
            port.receive(byte);
        }

        CHECK(uart.read(received) == 6);
        CHECK(received == std::array<uint8_t, 8>{1, 2, 3, 4, 5, 6, 0, 0});

        for (uint8_t byte = 7; byte <= 11; byte++) {
            port.receive(byte);
        }

        CHECK(uart.read(std::span{received}.first(3)) == 3);
        CHECK(received == std::array<uint8_t, 8>{7, 8, 9, 4, 5, 6, 0, 0});

        received = {};
        CHECK(uart.read(received) == 2);
        CHECK(received == std::array<uint8_t, 8>{10, 11, 0, 0, 0, 0, 0, 0});
        CHECK(uart.read(received) == 0);
    }

    TEST_CASE_FIXTURE(HostBoard, "hands the bytes to transmit to the port and refuses more until they leave") {
        hal::UartDma                 uart{uart_config};
        const std::array<uint8_t, 3> message{0x10, 0x20, 0x30};

        REQUIRE(uart.start_tx(message));
        CHECK(uart.is_transmitting());
        CHECK_FALSE(uart.start_tx(message));

        hal::host::UartPort& port = hal::host::Board::uart(uart_config.handle);
        CHECK(port.tx == std::deque<uint8_t>{0x10, 0x20, 0x30});

        port.tx.clear();
        CHECK_FALSE(uart.is_transmitting());
    }
}
}  // namespace micras::test
