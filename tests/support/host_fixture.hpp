/**
 * @file
 *
 * @brief The peripherals of the reference projects as the host tests configure them, and the fixture
 * that starts every test from a fresh board.
 */

#ifndef MICRAS_LIB_TESTS_HOST_FIXTURE_HPP
#define MICRAS_LIB_TESTS_HOST_FIXTURE_HPP

#include <cstdint>

#include <adc.h>
#include <crc.h>
#include <dma.h>
#include <fmac.h>
#include <gpio.h>
#include <main.h>
#include <spi.h>
#include <tim.h>
#include <usart.h>

#include "micras/hal/adc_dma.hpp"
#include "micras/hal/crc.hpp"
#include "micras/hal/encoder.hpp"
#include "micras/hal/fmac.hpp"
#include "micras/hal/gpio.hpp"
#include "micras/hal/host/board.hpp"
#include "micras/hal/host/clock.hpp"
#include "micras/hal/pwm.hpp"
#include "micras/hal/pwm_dma.hpp"
#include "micras/hal/spi.hpp"
#include "micras/hal/timer.hpp"
#include "micras/hal/uart_dma.hpp"

namespace micras::test {
/**
 * @brief The output pin of the reference projects.
 */
inline const hal::Gpio::Config status_led_config{
    .port = Status_LED_GPIO_Port,
    .pin = Status_LED_Pin,
};

/**
 * @brief The input pin of the reference projects.
 */
inline const hal::Gpio::Config button_config{
    .port = Button_GPIO_Port,
    .pin = Button_Pin,
};

/**
 * @brief The SPI bus and its chip select, in SPI mode 3.
 */
inline const hal::Spi::Config spi_config{
    .init_function = MX_SPI3_Init,
    .handle = &hspi3,
    .cs_gpio = {.port = SPI_CSn_GPIO_Port, .pin = SPI_CSn_Pin},
    .timeout = 2,
    .clock_polarity = SPI_POLARITY_HIGH,
    .clock_phase = SPI_PHASE_2EDGE,
};

/**
 * @brief A second device on the same bus, in SPI mode 1, behind the output pin used as a chip select.
 */
inline const hal::Spi::Config second_spi_config{
    .init_function = MX_SPI3_Init,
    .handle = &hspi3,
    .cs_gpio = status_led_config,
    .timeout = 2,
    .clock_polarity = SPI_POLARITY_LOW,
    .clock_phase = SPI_PHASE_2EDGE,
};

/**
 * @brief The PWM channel.
 */
inline const hal::Pwm::Config pwm_config{
    .init_function = MX_TIM15_Init,
    .handle = &htim15,
    .timer_channel = TIM_CHANNEL_1,
    .inverted = false,
};

/**
 * @brief The PWM channel a DMA stream feeds.
 */
inline const hal::PwmDma::Config pwm_dma_config{
    .init_function = MX_TIM8_Init,
    .handle = &htim8,
    .timer_channel = TIM_CHANNEL_1,
};

/**
 * @brief The timer in encoder mode.
 */
inline const hal::Encoder::Config encoder_config{
    .init_function = MX_TIM5_Init,
    .handle = &htim5,
    .timer_channel = TIM_CHANNEL_ALL,
};

/**
 * @brief The UART.
 */
inline const hal::UartDma::Config uart_config{
    .init_function = MX_UART4_Init,
    .handle = &huart4,
};

/**
 * @brief The ADC, twelve bits referenced to 3.3 V.
 */
inline const hal::AdcDma::Config adc_config{
    .init_function = MX_ADC3_Init,
    .handle = &hadc3,
    .max_reading = 4095,
    .reference_voltage = 3.3F,
};

/**
 * @brief The CRC unit, set up for the AS5047U frames.
 */
inline const hal::Crc::Config crc_config{
    .handle = &hcrc,
};

/**
 * @brief The filter math accelerator.
 */
inline const hal::Fmac::Config fmac_config{
    .init_function = MX_FMAC_Init,
    .handle = &hfmac,
};

/**
 * @brief Starts a test from the state the board has after a reset.
 *
 * @note Forgets every port and rewinds the clock, as the host backend asks of independent runs in one
 * process, and returns every handle to its reset state, so that each driver runs its init function
 * again. The pins are named, and the SPI bus initialized, before any driver creates a port for them,
 * since a port takes its name when it is created.
 */
class HostBoard {
public:
    /**
     * @brief Reset the board, the clock and the handles.
     */
    HostBoard() {
        hal::host::Board::reset();
        hal::host::Clock::instance().reset();
        hal::host::Clock::instance().configure(SystemCoreClock / 1000000);
        hal::Timer::init();

        hadc3 = {};
        hcrc = {};
        hfmac = {};
        hspi3 = {};
        htim2 = {};
        htim5 = {};
        htim8 = {};
        htim15 = {};
        huart4 = {};

        MX_GPIO_Init();
        MX_DMA_Init();
        MX_CRC_Init();
        MX_SPI3_Init();
    }

    /**
     * @brief Advance the host clock as a firmware waiting that long does, one timer read at a time.
     *
     * @param microseconds How long to wait.
     */
    static void wait_us(uint32_t microseconds) {
        const uint32_t start = hal::Timer::get_counter();

        while (hal::Timer::to_microseconds(hal::Timer::get_counter() - start) < microseconds) { }
    }
};
}  // namespace micras::test

#endif  // MICRAS_LIB_TESTS_HOST_FIXTURE_HPP
