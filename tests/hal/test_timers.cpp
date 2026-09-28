/**
 * @file
 */

#include <array>
#include <cstdint>

#include <doctest/doctest.h>

#include "host_fixture.hpp"
#include "micras/hal/encoder.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/host/ports.hpp"
#include "micras/hal/pwm.hpp"
#include "micras/hal/pwm_dma.hpp"
#include "tim.h"

namespace micras::test {
TEST_SUITE("timers") {
    TEST_CASE_FIXTURE(HostBoard, "runs a PWM channel at the frequency its registers give") {
        const hal::Pwm pwm{pwm_config};

        CHECK(pwm.was_initialized());
        CHECK(static_cast<double>(pwm.get_frequency()) == doctest::Approx(1000.0));
        CHECK(
            static_cast<double>(hal::host::Board::pwm(pwm_config.handle, pwm_config.timer_channel).frequency) ==
            doctest::Approx(1000.0)
        );
    }

    TEST_CASE_FIXTURE(HostBoard, "reports the duty cycle the compare register gives") {
        hal::Pwm                  pwm{pwm_config};
        const hal::host::PwmPort& port = hal::host::Board::pwm(pwm_config.handle, pwm_config.timer_channel);

        pwm.set_duty_cycle(25.0F);
        CHECK(static_cast<double>(port.duty_cycle) == doctest::Approx(25.0));
        CHECK(port.touched);

        pwm.set_duty_cycle(150.0F);
        CHECK(static_cast<double>(port.duty_cycle) == doctest::Approx(100.0));

        pwm.set_duty_cycle(-10.0F);
        CHECK(static_cast<double>(port.duty_cycle) == doctest::Approx(0.0));
    }

    TEST_CASE_FIXTURE(HostBoard, "reports the active fraction of an inverted channel") {
        hal::Pwm::Config config = pwm_config;
        config.inverted = true;
        hal::Pwm pwm{config};

        pwm.set_duty_cycle(30.0F);

        CHECK(
            static_cast<double>(hal::host::Board::pwm(config.handle, config.timer_channel).duty_cycle) ==
            doctest::Approx(30.0)
        );
        CHECK((htim15.Instance->CCER & TIM_CCER_CC1P) != 0);
    }

    TEST_CASE_FIXTURE(HostBoard, "changes the frequency through the autoreload register") {
        hal::Pwm pwm{pwm_config};

        pwm.set_frequency(2000);

        CHECK(htim15.Instance->ARR == 499);
        CHECK(static_cast<double>(pwm.get_frequency()) == doctest::Approx(2000.0));
        CHECK(
            static_cast<double>(hal::host::Board::pwm(pwm_config.handle, pwm_config.timer_channel).frequency) ==
            doctest::Approx(2000.0)
        );
    }

    TEST_CASE_FIXTURE(HostBoard, "keeps a DMA transfer of compare values busy for one period per value") {
        hal::PwmDma             pwm_dma{pwm_dma_config};
        std::array<uint16_t, 8> compares{};
        compares.fill(pwm_dma.get_compare(50.0F));

        CHECK(pwm_dma.was_initialized());
        CHECK(compares.front() == 125);

        pwm_dma.start_dma(compares);
        CHECK(pwm_dma.is_busy());
        CHECK(hal::host::Board::pwm_dma(pwm_dma_config.handle, pwm_dma_config.timer_channel).compares.size() == 8);

        wait_us(7);
        CHECK(pwm_dma.is_busy());

        wait_us(2);
        CHECK_FALSE(pwm_dma.is_busy());
    }

    TEST_CASE_FIXTURE(HostBoard, "counts encoder pulses from where the encoder started") {
        hal::host::EncoderPort& port = hal::host::Board::encoder(encoder_config.handle);
        port.count = 1000;

        const hal::Encoder encoder{encoder_config};
        CHECK(encoder.was_initialized());
        CHECK(encoder.get_counter() == 0);

        port.count = 900;
        CHECK(encoder.get_counter() == -100);
    }
}
}  // namespace micras::test
