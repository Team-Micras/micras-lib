/**
 * @file
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/fmac.hpp"

namespace micras::test {
namespace {
constexpr double q15_scale{32768.0};

constexpr std::array<int16_t, 3> feed_forward{2048, 4096, 2048};

constexpr std::array<int16_t, 2> feedback{29491, -13107};

std::vector<double> direct_form(const std::vector<double>& inputs) {
    std::vector<double>   outputs;
    std::array<double, 3> input_history{};
    std::array<double, 2> output_history{};

    for (const double input : inputs) {
        input_history = {input, input_history.at(0), input_history.at(1)};

        double output = 0.0;

        for (std::size_t tap = 0; tap < feed_forward.size(); tap++) {
            output += feed_forward.at(tap) / q15_scale * input_history.at(tap);
        }

        for (std::size_t tap = 0; tap < feedback.size(); tap++) {
            output += feedback.at(tap) / q15_scale * output_history.at(tap);
        }

        output_history = {output, output_history.at(0)};
        outputs.push_back(output);
    }

    return outputs;
}
}  // namespace

TEST_SUITE("fmac") {
    TEST_CASE_FIXTURE(HostBoard, "runs the configured filter as a direct form reference does") {
        hal::Fmac fmac{fmac_config};
        REQUIRE(fmac.configure_iir(feed_forward, feedback));
        CHECK(fmac.was_initialized());

        std::vector<double> inputs;

        for (int sample = 0; sample < 200; sample++) {
            inputs.push_back(0.5 * std::sin(0.1 * sample) + (sample > 100 ? 0.25 : 0.0));
        }

        const std::vector<double> expected = direct_form(inputs);

        for (std::size_t sample = 0; sample < inputs.size(); sample++) {
            CAPTURE(sample);
            const auto   input = static_cast<int16_t>(std::lround(inputs.at(sample) * q15_scale));
            const double output = fmac.update(input) / q15_scale;

            CHECK(std::abs(output - expected.at(sample)) < 1e-3);
        }
    }

    TEST_CASE_FIXTURE(HostBoard, "clips the output to the q1.15 range") {
        hal::Fmac                        fmac{fmac_config};
        constexpr std::array<int16_t, 1> gain{32767};
        constexpr std::array<int16_t, 1> integrator{32767};
        REQUIRE(fmac.configure_iir(gain, integrator));

        int16_t output = 0;

        for (int sample = 0; sample < 10; sample++) {
            output = fmac.update(30000);
        }

        CHECK(output == 32767);
    }

    TEST_CASE_FIXTURE(HostBoard, "refuses a filter that does not fit its buffers") {
        hal::Fmac                        fmac{fmac_config};
        constexpr std::array<int16_t, 5> too_many{};

        CHECK_FALSE(fmac.configure_iir(too_many, too_many));
        CHECK_FALSE(fmac.configure_iir({}, feedback));
        CHECK_FALSE(fmac.was_initialized());
    }
}
}  // namespace micras::test
