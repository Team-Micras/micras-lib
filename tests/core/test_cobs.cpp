/**
 * @file
 */

#include <cstdint>
#include <random>
#include <vector>

#include <doctest/doctest.h>

#include "micras/core/cobs.hpp"

namespace micras::core {
namespace {
using Bytes = std::vector<uint8_t>;
}  // namespace

static Bytes encode(const Bytes& raw) {
    Bytes encoded(cobs_encoded_size(raw.size()));
    encoded.resize(cobs_encode(raw, encoded));
    return encoded;
}

static Bytes decode(const Bytes& encoded) {
    Bytes decoded(encoded.size() + 1);
    decoded.resize(cobs_decode(encoded, decoded));
    return decoded;
}

static Bytes count(int first, int last) {
    Bytes bytes;

    for (int value = first; value <= last; value++) {
        bytes.push_back(static_cast<uint8_t>(value));
    }

    return bytes;
}

static Bytes join(const Bytes& head, const Bytes& tail) {
    Bytes joined = head;
    joined.insert(joined.end(), tail.begin(), tail.end());
    return joined;
}

static void check_vector(const Bytes& raw, const Bytes& expected) {
    CAPTURE(raw.size());

    const Bytes encoded = encode(raw);

    CHECK(encoded == expected);
    CHECK(decode(encoded) == raw);
}

TEST_SUITE("cobs") {
    TEST_CASE("encodes the reference vectors and decodes them back") {
        check_vector({}, {0x01});
        check_vector({0x00}, {0x01, 0x01});
        check_vector({0x00, 0x00}, {0x01, 0x01, 0x01});
        check_vector({0x00, 0x11, 0x00}, {0x01, 0x02, 0x11, 0x01});
        check_vector({0x11, 0x22, 0x00, 0x33}, {0x03, 0x11, 0x22, 0x02, 0x33});
        check_vector({0x11, 0x22, 0x33, 0x44}, {0x05, 0x11, 0x22, 0x33, 0x44});
        check_vector({0x11, 0x00, 0x00, 0x00}, {0x02, 0x11, 0x01, 0x01, 0x01});
        check_vector(count(1, 254), join({0xFF}, count(1, 254)));
        check_vector(count(0, 254), join({0x01, 0xFF}, count(1, 254)));
        check_vector(count(1, 255), join(join({0xFF}, count(1, 254)), {0x02, 0xFF}));
        check_vector(join(count(2, 255), {0x00}), join(join({0xFF}, count(2, 255)), {0x01, 0x01}));
        check_vector(join(count(3, 255), {0x00, 0x01}), join(join({0xFE}, count(3, 255)), {0x02, 0x01}));
    }

    TEST_CASE("round trips random frames without a delimiter in the encoding") {
        // NOLINTNEXTLINE(bugprone-random-generator-seed) a fixed seed keeps the test reproducible
        std::mt19937 generator{7};

        for (int trial = 0; trial < 200000; trial++) {
            Bytes          raw(generator() % 600);
            const uint32_t alphabet = trial % 3 == 0 ? 3 : 256;

            for (uint8_t& byte : raw) {
                byte = static_cast<uint8_t>(generator() % alphabet);
            }

            const Bytes encoded = encode(raw);
            bool        delimiter_free = true;

            for (const uint8_t byte : encoded) {
                delimiter_free = delimiter_free and byte != cobs_delimiter;
            }

            REQUIRE(encoded.size() <= cobs_encoded_size(raw.size()));
            REQUIRE(delimiter_free);
            REQUIRE(decode(encoded) == raw);
        }
    }

    TEST_CASE("rejects malformed frames instead of decoding half of them") {
        Bytes scratch(8);
        CHECK(cobs_decode(Bytes{0x00}, scratch) == 0);
        CHECK(cobs_decode(Bytes{0x05, 0x01}, scratch) == 0);

        Bytes small(2);
        CHECK(cobs_decode(Bytes{0x06, 1, 2, 3, 4, 5}, small) == 0);
    }
}
}  // namespace micras::core
