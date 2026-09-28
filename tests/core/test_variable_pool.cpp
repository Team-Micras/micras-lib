/**
 * @file
 */

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <span>
#include <vector>

#include <doctest/doctest.h>

#include "micras/core/serializable.hpp"
#include "micras/core/variable_pool.hpp"

namespace micras::core {
namespace {
class Blob : public ISerializable {
public:
    std::vector<uint8_t> serialize() const override { return this->data; }

    void deserialize(const uint8_t* buffer, uint16_t size) override {
        this->data.assign(buffer, std::next(buffer, size));
    }

private:
    std::vector<uint8_t> data{1, 2, 3};
};

enum class Profile : uint8_t {
    A,
    B
};
}  // namespace

static std::span<const uint8_t> bytes_of(const float& value) {
    return {std::bit_cast<const uint8_t*>(&value), sizeof(value)};
}

TEST_SUITE("variable_pool") {
    TEST_CASE("registers finds reads and writes variables with their access") {
        TVariablePool<8> pool;
        float            speed = 1.5F;
        const float      gain = 2.0F;
        Profile          profile = Profile::A;
        Blob             blob;

        const auto speed_id = pool.add("speed/", "linear", speed, {.stream = true, .write = true});
        const auto gain_id =
            pool.add("model/", "kv", gain, {.stream = true, .write = true, .idle = true, .persist = true});
        const auto profile_id = pool.add("", "run_profile", profile, {.write = true, .idle = true, .persist = true});
        const auto blob_id = pool.add("", "maze", blob, {.persist = true});

        CHECK(speed_id == 0);
        CHECK(gain_id == 1);
        CHECK(profile_id == 2);
        CHECK(blob_id == 3);
        CHECK(pool.all().size() == 4);
        CHECK_FALSE(pool.at(gain_id).access.write);
        CHECK(pool.at(profile_id).type == TypeCode::U8);
        CHECK(pool.at(blob_id).type == TypeCode::BLOB);
        CHECK_FALSE(pool.at(blob_id).access.stream);

        CHECK(pool.find("speed/linear") == speed_id);
        CHECK(pool.find("model/kv") == gain_id);
        CHECK(pool.find("run_profile") == profile_id);
        CHECK_FALSE(pool.find("speed/linea").has_value());
        CHECK_FALSE(pool.find("speed/linearr").has_value());

        std::array<uint8_t, 8> buffer{};
        CHECK(pool.read(speed_id, buffer) == sizeof(float));

        float read_back{};
        std::memcpy(&read_back, buffer.data(), sizeof(read_back));
        CHECK(read_back == 1.5F);
        CHECK(pool.read(blob_id, buffer) == 0);

        const float                  written = 9.0F;
        const std::array<uint8_t, 1> one{1};

        CHECK(pool.write(speed_id, bytes_of(written), false) == VariablePool::WriteStatus::OK);
        CHECK(speed == 9.0F);
        CHECK(pool.write(gain_id, bytes_of(written), true) == VariablePool::WriteStatus::READ_ONLY);
        CHECK(pool.write(profile_id, one, false) == VariablePool::WriteStatus::NEEDS_IDLE);
        CHECK(pool.write(profile_id, one, true) == VariablePool::WriteStatus::OK);
        CHECK(profile == Profile::B);
        CHECK(pool.write(speed_id, one, true) == VariablePool::WriteStatus::WRONG_SIZE);
        CHECK(pool.write(99, one, true) == VariablePool::WriteStatus::NO_SUCH_ID);
    }

    TEST_CASE("hashes the schema by the names types and access") {
        float       speed = 1.5F;
        const float gain = 2.0F;
        Profile     profile = Profile::A;
        Blob        blob;

        const auto fill = [&](VariablePool& pool) {
            pool.add("speed/", "linear", speed, {.stream = true, .write = true});
            pool.add("model/", "kv", gain, {.stream = true, .write = true, .idle = true, .persist = true});
            pool.add("", "run_profile", profile, {.write = true, .idle = true, .persist = true});
            pool.add("", "maze", blob, {.persist = true});
        };

        TVariablePool<8> first;
        TVariablePool<8> second;
        fill(first);
        fill(second);

        CHECK(second.schema_hash() == first.schema_hash());

        second.add("", "extra", speed, {});
        CHECK(second.schema_hash() != first.schema_hash());
    }

    TEST_CASE("refuses a variable once the pool is full") {
        TVariablePool<1> pool;
        float            value = 0.0F;

        pool.add("", "a", value, {});

        CHECK(pool.add("", "b", value, {}) == VariablePool::invalid_id);
        CHECK(pool.all().size() == 1);
    }
}
}  // namespace micras::core
