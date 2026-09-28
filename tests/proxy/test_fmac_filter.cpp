/**
 * @file
 */

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/core/butterworth_filter.hpp"
#include "micras/proxy/fmac_filter.hpp"

namespace micras::test {
namespace {
const core::ButterworthFilter::Config filter_config{
    .cutoff_frequency = 200.0F,
    .sampling_frequency = 1000.0F,
};

std::vector<double> direct_form(const std::vector<double>& inputs) {
    const core::ButterworthFilter::Coefficients coefficients =
        core::ButterworthFilter::compute_coefficients(filter_config);
    std::array<double, 3> input_history{};
    std::array<double, 2> output_history{};
    std::vector<double>   outputs;

    for (const double input : inputs) {
        input_history = {input, input_history.at(0), input_history.at(1)};

        double output = 0.0;

        for (std::size_t tap = 0; tap < input_history.size(); tap++) {
            output += static_cast<double>(coefficients.feed_forward.at(tap)) * input_history.at(tap);
        }

        for (std::size_t tap = 0; tap < output_history.size(); tap++) {
            output -= static_cast<double>(coefficients.feedback.at(tap)) * output_history.at(tap);
        }

        output_history = {output, output_history.at(0)};
        outputs.push_back(output);
    }

    return outputs;
}
}  // namespace

TEST_SUITE("fmac_filter") {
    TEST_CASE_FIXTURE(HostBoard, "filters as the Butterworth filter it was designed as") {
        proxy::FmacFilter filter{{.fmac = fmac_config, .filter = filter_config}};
        REQUIRE(filter.was_initialized());

        std::vector<double> inputs;

        for (int sample = 0; sample < 300; sample++) {
            inputs.push_back((sample < 150 ? 0.5 : -0.25) + 0.1 * std::sin(0.9 * sample));
        }

        const std::vector<double> expected = direct_form(inputs);

        for (std::size_t sample = 0; sample < inputs.size(); sample++) {
            CAPTURE(sample);
            const double output = filter.update(static_cast<float>(inputs.at(sample)));

            CHECK(std::abs(output - expected.at(sample)) < 2e-3);
            CHECK(filter.get_last() == static_cast<float>(output));
        }
    }

    TEST_CASE_FIXTURE(HostBoard, "stays off for a filter whose coefficients q1.15 cannot hold") {
        proxy::FmacFilter filter{
            {.fmac = fmac_config, .filter = {.cutoff_frequency = 50.0F, .sampling_frequency = 1000.0F}}
        };

        CHECK_FALSE(filter.was_initialized());
        CHECK(filter.update(0.5F) == 0.0F);
    }
}
}  // namespace micras::test
