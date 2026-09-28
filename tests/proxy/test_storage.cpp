/**
 * @file
 */

#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/core/serializable.hpp"
#include "micras/core/variable_pool.hpp"
#include "micras/hal/flash.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/proxy/storage.hpp"

namespace micras::test {
namespace {
class Blob : public core::ISerializable {
public:
    Blob() = default;

    explicit Blob(std::vector<uint8_t> data) : data{std::move(data)} { }

    std::vector<uint8_t> serialize() const override { return this->data; }

    void deserialize(const uint8_t* buffer, uint16_t size) override {
        this->data.assign(buffer, std::next(buffer, size));
    }

    const std::vector<uint8_t>& get() const { return this->data; }

private:
    std::vector<uint8_t> data;
};

const proxy::Storage::Config storage_config{.start_sector = 0, .number_of_sectors = 1};

class StoredPool : public HostBoard {
public:
    StoredPool() {
        core::TVariablePool<8> pool;
        float                  gain = 3.25F;
        uint8_t                profile = 7;
        bool                   flag = true;
        float                  transient = 1.0F;
        Blob                   maze{{9, 8, 7, 6, 5}};

        pool.add("model/", "kv", gain, {.persist = true});
        pool.add("", "run_profile", profile, {.persist = true});
        pool.add("", "flag", flag, {.persist = true});
        pool.add("", "transient", transient, {.stream = true});
        pool.add("", "maze", maze, {.persist = true});

        proxy::Storage storage{storage_config};
        CHECK_FALSE(storage.is_valid());
        CHECK(storage.restore(pool) == 0);
        CHECK(storage.save(pool));
        CHECK(storage.is_valid());
    }
};
}  // namespace

TEST_SUITE("storage") {
    TEST_CASE_FIXTURE(StoredPool, "restores every persistent variable and leaves the others alone") {
        core::TVariablePool<8> pool;
        float                  gain = 0.0F;
        uint8_t                profile = 0;
        bool                   flag = false;
        float                  transient = 42.0F;
        Blob                   maze;

        pool.add("model/", "kv", gain, {.persist = true});
        pool.add("", "run_profile", profile, {.persist = true});
        pool.add("", "flag", flag, {.persist = true});
        pool.add("", "transient", transient, {.stream = true});
        pool.add("", "maze", maze, {.persist = true});

        proxy::Storage storage{storage_config};
        CHECK(storage.is_valid());
        CHECK(storage.restore(pool) == 4);
        CHECK(gain == 3.25F);
        CHECK(profile == 7);
        CHECK(flag);
        CHECK(maze.get() == std::vector<uint8_t>{9, 8, 7, 6, 5});
        CHECK(transient == 42.0F);
    }

    TEST_CASE_FIXTURE(StoredPool, "finds the variables it knows in a reordered pool") {
        core::TVariablePool<8> pool;
        uint8_t                profile = 0;
        float                  gain = 0.0F;
        float                  fresh = 1.0F;

        pool.add("", "fresh", fresh, {.persist = true});
        pool.add("", "run_profile", profile, {.persist = true});
        pool.add("model/", "kv", gain, {.persist = true});

        proxy::Storage storage{storage_config};
        CHECK(storage.restore(pool) == 2);
        CHECK(profile == 7);
        CHECK(gain == 3.25F);
        CHECK(fresh == 1.0F);
    }

    TEST_CASE_FIXTURE(StoredPool, "refuses a variable whose type changed instead of misreading it") {
        core::TVariablePool<8> pool;
        uint32_t               gain = 0;
        pool.add("model/", "kv", gain, {.persist = true});

        proxy::Storage storage{storage_config};
        CHECK(storage.restore(pool) == 0);
        CHECK(gain == 0);
    }

    TEST_CASE_FIXTURE(StoredPool, "leaves no image to restore after a write the power cut off") {
        core::TVariablePool<8> pool;
        float                  gain = 99.0F;
        pool.add("model/", "kv", gain, {.persist = true});

        proxy::Storage storage{storage_config};
        hal::host::Board::flash().write_budget = 0;
        CHECK_FALSE(storage.save(pool));

        const proxy::Storage after{storage_config};
        CHECK_FALSE(after.is_valid());
    }

    TEST_CASE_FIXTURE(HostBoard, "treats a body written without its header as never saved") {
        core::TVariablePool<8> pool;
        float                  gain = 1.0F;
        pool.add("model/", "kv", gain, {.persist = true});

        proxy::Storage storage{storage_config};
        hal::host::Board::flash().write_budget = 1;
        CHECK_FALSE(storage.save(pool));

        const proxy::Storage after{storage_config};
        CHECK_FALSE(after.is_valid());
    }
}
}  // namespace micras::test
