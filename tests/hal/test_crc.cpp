/**
 * @file
 */

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/crc.hpp"

namespace micras::test {
namespace {
uint8_t reference_crc8(std::span<const uint8_t> data, uint8_t polynomial, uint8_t initial) {
    uint8_t crc = initial;

    for (const uint8_t byte : data) {
        crc ^= byte;

        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80U) != 0 ? static_cast<uint8_t>((crc << 1U) ^ polynomial) : static_cast<uint8_t>(crc << 1U);
        }
    }

    return crc;
}
}  // namespace

TEST_SUITE("crc") {
    TEST_CASE_FIXTURE(HostBoard, "computes the check value of CRC-8/SAE-J1850") {
        CRC_HandleTypeDef handle{};
        handle.Init = {
            .DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE,
            .DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE,
            .GeneratingPolynomial = 0x1D,
            .CRCLength = CRC_POLYLENGTH_8B,
            .InitValue = 0xFF,
            .InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE,
            .OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE,
        };
        hal::Crc crc{{.handle = &handle}};

        constexpr std::string_view check{"123456789"};
        const std::vector<uint8_t> bytes(check.begin(), check.end());

        CHECK((crc.calculate(bytes) ^ 0xFFU) == 0x4B);
    }

    TEST_CASE_FIXTURE(HostBoard, "computes what the unit is configured for") {
        hal::Crc crc{crc_config};

        for (const std::array<uint8_t, 2> bytes : std::vector<std::array<uint8_t, 2>>{
                 {0x00, 0x00}, {0x40, 0x01}, {0x40, 0x1A}, {0x00, 0x80}, {0x3F, 0xFF}, {0xC0, 0x15}
             }) {
            CAPTURE(bytes);
            CHECK(crc.calculate(bytes) == reference_crc8(bytes, 0x1D, 0xC4));
        }
    }

    TEST_CASE_FIXTURE(HostBoard, "computes the standard CRC-32 with the default polynomial") {
        CRC_HandleTypeDef handle{};
        handle.Init = {
            .DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE,
            .DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE,
            .GeneratingPolynomial = 0,
            .CRCLength = CRC_POLYLENGTH_32B,
            .InitValue = 0,
            .InputDataInversionMode = CRC_INPUTDATA_INVERSION_BYTE,
            .OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE,
        };
        hal::Crc crc{{.handle = &handle}};

        constexpr std::string_view check{"123456789"};
        const std::vector<uint8_t> bytes(check.begin(), check.end());

        CHECK((crc.calculate(bytes) ^ 0xFFFFFFFFU) == 0xCBF43926U);
    }
}
}  // namespace micras::test
