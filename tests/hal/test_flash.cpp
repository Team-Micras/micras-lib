/**
 * @file
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/flash.hpp"
#include "micras/hal/host/board.hpp"

namespace micras::test {
namespace {
constexpr std::array<uint8_t, 5> data{1, 2, 3, 4, 5};
}  // namespace

static bool is_erased(std::span<const uint8_t> bytes) {
    return std::ranges::all_of(bytes, [](uint8_t byte) { return byte == hal::FlashWord::erased_value; });
}

TEST_SUITE("flash") {
    TEST_CASE_FIXTURE(HostBoard, "reserves the upper half of the sectors") {
        CHECK(hal::Flash::sector_size == 128 * 1024);
        CHECK(hal::Flash::total_sectors == 4);
        CHECK(hal::FlashWord::size == 32);
        CHECK(is_erased(hal::Flash::read(0, hal::Flash::total_size)));
    }

    TEST_CASE_FIXTURE(HostBoard, "writes a flash word once and reads it back padded") {
        REQUIRE(hal::Flash::write(1, 0, data) == hal::Flash::Status::OK);

        const std::span<const uint8_t> word = hal::Flash::read(1, 0, hal::FlashWord::size);
        CHECK(std::ranges::equal(word.first(data.size()), data));
        CHECK(is_erased(word.subspan(data.size())));
        CHECK(hal::host::Board::flash().touched);

        CHECK(hal::Flash::write(1, 0, data) == hal::Flash::Status::ERROR);
    }

    TEST_CASE_FIXTURE(HostBoard, "writes again after the sector is erased") {
        REQUIRE(hal::Flash::write(0, 0, data) == hal::Flash::Status::OK);
        REQUIRE(hal::Flash::erase_sectors(0) == hal::Flash::Status::OK);

        CHECK(is_erased(hal::Flash::read(0, 0, hal::FlashWord::size)));
        CHECK(hal::Flash::write(0, 0, data) == hal::Flash::Status::OK);
    }

    TEST_CASE_FIXTURE(HostBoard, "refuses misaligned and out of bounds accesses") {
        CHECK(hal::Flash::write(0, 4, data) == hal::Flash::Status::MISALIGNED);
        CHECK(hal::Flash::write(hal::Flash::total_sectors, 0, data) == hal::Flash::Status::OUT_OF_BOUNDS);
        CHECK(hal::Flash::write(hal::Flash::total_size, data) == hal::Flash::Status::OUT_OF_BOUNDS);
        CHECK(hal::Flash::erase_sectors(3, 2) == hal::Flash::Status::OUT_OF_BOUNDS);
        CHECK(hal::Flash::read(hal::Flash::total_size - 1, 2).empty());
    }

    TEST_CASE_FIXTURE(HostBoard, "stops a write where the simulated power loss cuts it off") {
        std::array<uint8_t, static_cast<std::size_t>(3 * hal::FlashWord::size)> long_data{};
        long_data.fill(0x42);
        hal::host::Board::flash().write_budget = 2;

        CHECK(hal::Flash::write(0, 0, long_data) == hal::Flash::Status::ERROR);
        CHECK_FALSE(is_erased(hal::Flash::read(0, 0, 2 * hal::FlashWord::size)));
        CHECK(is_erased(hal::Flash::read(0, 2 * hal::FlashWord::size, hal::FlashWord::size)));
    }
}
}  // namespace micras::test
