/**
 * @file
 */

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include "micras/comm/frame.hpp"
#include "micras/comm/link.hpp"
#include "micras/comm/protocol.hpp"
#include "micras/core/byte_stream.hpp"
#include "micras/core/variable_pool.hpp"

namespace micras::comm {
namespace {
using Bytes = std::vector<uint8_t>;

class Loopback : public core::IByteStream {
public:
    std::size_t read(std::span<uint8_t> into) override {
        const std::size_t count = std::min(into.size(), this->to_robot.size());

        for (std::size_t index = 0; index < count; index++) {
            into[index] = this->to_robot.front();
            this->to_robot.pop_front();
        }

        return count;
    }

    std::size_t write(std::span<const uint8_t> from) override {
        if (capacity - this->from_robot.size() < from.size()) {
            return 0;
        }

        this->from_robot.insert(this->from_robot.end(), from.begin(), from.end());
        return from.size();
    }

    std::deque<uint8_t>& incoming() { return this->to_robot; }

    std::deque<uint8_t>& outgoing() { return this->from_robot; }

private:
    static constexpr std::size_t capacity{4096};

    std::deque<uint8_t> to_robot;
    std::deque<uint8_t> from_robot;
};

class Commands : public ICommandHandler {
public:
    static constexpr uint8_t known_command{42};

    CommandResult handle_command(uint8_t code, uint32_t argument) override {
        this->received.emplace_back(code, argument);
        return code == known_command ? CommandResult::OK : CommandResult::UNKNOWN;
    }

    const std::vector<std::pair<uint8_t, uint32_t>>& seen() const { return this->received; }

private:
    std::vector<std::pair<uint8_t, uint32_t>> received;
};

struct Message {
    MessageType type;
    Bytes       payload;
};

float to_float(uint32_t bits) {
    return std::bit_cast<float>(bits);
}

std::vector<Message> of_type(const std::vector<Message>& messages, MessageType type) {
    std::vector<Message> matching;
    std::ranges::copy_if(messages, std::back_inserter(matching), [type](const Message& message) {
        return message.type == type;
    });
    return matching;
}

class Session {
public:
    static constexpr uint32_t loop_time_us{125};

    Session() {
        this->pool.add("cmd/", "linear", this->linear, {.stream = true});
        this->pool.add("cmd/", "angular", this->angular, {.stream = true});
        this->pool.add("model/", "gain", this->gain, {.stream = true, .write = true, .idle = true, .persist = true});
        this->pool.add("", "run_profile", this->profile, {.stream = true, .write = true});
    }

    void send(MessageType type, const Bytes& payload) {
        Bytes frame(max_frame_size);
        frame.resize(encode_frame(type, payload, frame));
        REQUIRE_FALSE(frame.empty());
        this->io.incoming().insert(this->io.incoming().end(), frame.begin(), frame.end());
    }

    void run(int iterations, bool idle = true) {
        for (int iteration = 0; iteration < iterations; iteration++) {
            this->now += loop_time_us;
            this->link.poll(idle);
            this->link.pump(this->now);
        }
    }

    std::vector<Message> drain() {
        std::vector<Message> messages;
        const std::size_t    taken = this->io.outgoing().size();

        while (not this->io.outgoing().empty()) {
            const uint8_t byte = this->io.outgoing().front();
            this->io.outgoing().pop_front();

            if (this->application.push(byte)) {
                messages.push_back(
                    {this->application.type(),
                     Bytes(this->application.payload().begin(), this->application.payload().end())}
                );
            }
        }

        if (taken > 0) {
            this->send(MessageType::CREDIT, {static_cast<uint8_t>(taken), static_cast<uint8_t>(taken >> 8U)});
        }

        return messages;
    }

    Message only(MessageType type) {
        const std::vector<Message> matching = of_type(this->drain(), type);
        REQUIRE(matching.size() == 1);
        return matching.front();
    }

    uint32_t dropped_samples() {
        core::TVariablePool<8> probe;
        this->link.register_variables(probe, "link/");

        std::array<uint8_t, sizeof(uint32_t)> buffer{};
        probe.read(probe.find("link/dropped_samples").value(), buffer);
        return Reader{buffer}.u32();
    }

    core::TVariablePool<32> pool;
    float                   linear{1.0F};
    float                   angular{2.0F};
    float                   gain{0.5F};
    uint8_t                 profile{0};
    Commands                commands;
    Loopback                io;
    Link                    link{io, pool, commands, {.loop_time_us = loop_time_us}};
    FrameReader             application;
    uint32_t                now{0};
};
}  // namespace

TEST_SUITE("link") {
    TEST_CASE_FIXTURE(Session, "answers hello with the version the schema hash and the loop time") {
        this->send(MessageType::HELLO, {});
        this->run(4);

        const std::vector<Message> messages = this->drain();
        REQUIRE(messages.size() == 1);
        REQUIRE(messages.front().type == MessageType::HELLO_ACK);

        Reader reader{messages.front().payload};
        CHECK(reader.u8() == protocol_version);
        CHECK(reader.u32() == this->pool.schema_hash());
        CHECK(reader.u16() == 4);
        CHECK(reader.u32() == loop_time_us);
    }

    TEST_CASE_FIXTURE(Session, "sends the schema in pages") {
        this->send(MessageType::SCHEMA_REQUEST, {0, 0});
        this->run(40);

        std::map<uint16_t, std::string> names;

        for (const Message& message : this->drain()) {
            REQUIRE(message.type == MessageType::SCHEMA_PAGE);

            Reader reader{message.payload};
            CHECK(reader.u32() == this->pool.schema_hash());

            const uint16_t first = reader.u16();
            CHECK(reader.u16() == 4);

            const uint8_t count = reader.u8();

            for (uint8_t entry = 0; entry < count; entry++) {
                reader.u16();

                const uint8_t                  length = reader.u8();
                const std::span<const uint8_t> name = reader.rest().first(length);
                names[static_cast<uint16_t>(first + entry)] = std::string(name.begin(), name.end());

                for (uint8_t skipped = 0; skipped < length; skipped++) {
                    reader.u8();
                }
            }

            CHECK(reader.valid());
        }

        CHECK(names.size() == 4);
        CHECK(names[0] == "cmd/linear");
        CHECK(names[2] == "model/gain");
        CHECK(names[3] == "run_profile");
    }

    TEST_CASE_FIXTURE(Session, "streams a group at its period with coherent samples") {
        this->send(MessageType::GROUP_DEFINE, {0, 4, 0, 2, 0, 0, 1, 0});
        this->send(MessageType::GROUP_ENABLE, {0, 1});
        this->run(4);

        const std::vector<Message> acks = of_type(this->drain(), MessageType::GROUP_ACK);
        REQUIRE(acks.size() == 2);

        Reader ack{acks.front().payload};
        ack.u8();
        CHECK(ack.u16() == 4);
        CHECK(ack.u16() == 8);

        this->linear = 3.5F;
        this->angular = -1.25F;
        this->drain();
        this->run(16);

        const std::vector<Message> samples = this->drain();
        REQUIRE(samples.size() == 4);

        uint16_t first_sequence{};

        for (std::size_t index = 0; index < samples.size(); index++) {
            CAPTURE(index);
            REQUIRE(samples.at(index).type == MessageType::SAMPLE);

            Reader reader{samples.at(index).payload};
            CHECK(reader.u8() == 0);

            const uint16_t sequence = reader.u16();

            if (index == 0) {
                first_sequence = sequence;
            }

            CHECK(sequence == first_sequence + index);
            reader.u32();
            CHECK(to_float(reader.u32()) == 3.5F);
            CHECK(to_float(reader.u32()) == -1.25F);
        }
    }

    TEST_CASE_FIXTURE(Session, "refuses a guarded write while the robot moves and accepts it when idle") {
        const Bytes write_gain{2, 0, 0, 0, 0x80, 0x3F};

        this->send(MessageType::WRITE, write_gain);
        this->run(4, false);

        const Message refusal = this->only(MessageType::WRITE_ACK);
        Reader        refused{refusal.payload};
        refused.u16();
        CHECK(refused.u8() == static_cast<uint8_t>(core::VariablePool::WriteStatus::NEEDS_IDLE));
        CHECK(this->gain == 0.5F);

        this->send(MessageType::WRITE, write_gain);
        this->run(4, true);

        const Message acceptance = this->only(MessageType::WRITE_ACK);
        Reader        accepted{acceptance.payload};
        accepted.u16();
        CHECK(accepted.u8() == static_cast<uint8_t>(core::VariablePool::WriteStatus::OK));
        CHECK(this->gain == 1.0F);

        this->send(MessageType::READ, {2, 0});
        this->run(4);

        const Message answer = this->only(MessageType::VALUE);
        Reader        value{answer.payload};
        value.u16();
        CHECK(to_float(value.u32()) == 1.0F);
    }

    TEST_CASE_FIXTURE(Session, "acts on a command once") {
        this->send(MessageType::COMMAND, {Commands::known_command, 7, 0, 0, 0});
        this->run(8);

        const Message answer = this->only(MessageType::COMMAND_ACK);
        Reader        ack{answer.payload};
        ack.u8();
        CHECK(ack.u8() == static_cast<uint8_t>(CommandResult::OK));

        REQUIRE(this->commands.seen().size() == 1);
        CHECK(this->commands.seen().front().first == Commands::known_command);
        CHECK(this->commands.seen().front().second == 7);
    }

    TEST_CASE_FIXTURE(Session, "drops the samples a closed credit window cannot carry and counts them") {
        this->send(MessageType::HELLO, {});
        this->run(4);
        this->drain();
        this->send(MessageType::GROUP_DEFINE, {0, 1, 0, 2, 0, 0, 1, 0});
        this->send(MessageType::GROUP_ENABLE, {0, 1});
        this->run(4);
        this->drain();

        const uint32_t baseline = this->dropped_samples();
        constexpr int  iterations{400};

        this->run(iterations);

        const std::vector<Message> sent = of_type(this->drain(), MessageType::SAMPLE);
        REQUIRE_FALSE(sent.empty());
        CHECK(sent.size() < 40);
        CHECK(this->dropped_samples() - baseline + sent.size() == iterations);

        Reader last{sent.back().payload};
        last.u8();
        const uint16_t last_sent = last.u16();

        this->send(MessageType::CREDIT, {0x00, 0x01});
        this->run(8);

        const std::vector<Message> resumed = of_type(this->drain(), MessageType::SAMPLE);
        REQUIRE_FALSE(resumed.empty());

        Reader first{resumed.front().payload};
        first.u8();
        CHECK(first.u16() > last_sent + 1);
    }

    TEST_CASE_FIXTURE(Session, "loses only the frame a corrupted byte hits") {
        const std::size_t corrupt_at = this->io.incoming().size() + 2;

        this->send(MessageType::READ, {0, 0});
        this->io.incoming().at(corrupt_at) ^= 0xFFU;
        this->send(MessageType::READ, {1, 0});
        this->run(8);

        const std::vector<Message> values = of_type(this->drain(), MessageType::VALUE);
        REQUIRE(values.size() == 1);

        Reader reader{values.front().payload};
        CHECK(reader.u16() == 1);
    }
}
}  // namespace micras::comm
