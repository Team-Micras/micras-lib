/**
 * @file
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <adc.h>
#include <crc.h>
#include <dma.h>
#include <fmac.h>
#include <gpio.h>
#include <main.h>
#include <spi.h>
#include <stm32h7xx_ll_adc.h>
#include <tim.h>
#include <usart.h>

#include "micras/core/butterworth_filter.hpp"
#include "micras/core/variable_pool.hpp"
#include "micras/hal/adc_dma.hpp"
#include "micras/hal/crc.hpp"
#include "micras/hal/encoder.hpp"
#include "micras/hal/gpio.hpp"
#include "micras/hal/mcu.hpp"
#include "micras/hal/pwm.hpp"
#include "micras/hal/pwm_dma.hpp"
#include "micras/hal/spi.hpp"
#include "micras/hal/timer.hpp"
#include "micras/hal/uart_dma.hpp"
#include "micras/proxy/fmac_filter.hpp"
#include "micras/proxy/storage.hpp"
#include "micras/proxy/watchdog.hpp"

extern "C" {
void SystemClock_Config();
}

namespace {
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool     storage_passed{};
volatile bool     watchdog_passed{};
volatile float    timer_clock_error{};
volatile bool     timer_passed{};
volatile uint32_t crc_value{};
volatile bool     crc_passed{};
volatile float    fmac_error{};
volatile bool     fmac_passed{};
volatile uint32_t vdda_mv{};
volatile bool     adc_passed{};
volatile bool     bench_done{};
volatile bool     start_pin_drivers{};

std::array<uint16_t, 2> adc_buffer{};
std::array<uint16_t, 2> adc_snapshot{};
std::array<uint16_t, 8> pwm_dma_compares{};
std::array<uint8_t, 17> spi_transmitted{};
std::array<uint8_t, 17> spi_received{};
std::array<uint8_t, 64> uart_ring{};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

constexpr uint32_t storage_magic{0x5EED1E55};

constexpr micras::proxy::Storage::Config storage_config{.start_sector = 0, .number_of_sectors = 1};

constexpr uint32_t watchdog_timeout_ms{50};

constexpr uint32_t timer_window_us{10000};

constexpr float timer_tolerance{1e-3F};

constexpr uint8_t crc_polynomial{0x1D};

constexpr uint8_t crc_initial_value{0xC4};

constexpr micras::core::ButterworthFilter::Config filter_config{
    .cutoff_frequency = 200.0F,
    .sampling_frequency = 1000.0F,
};

constexpr float fmac_tolerance{2e-3F};

constexpr uint32_t adc_timeout_us{10000};

constexpr uint32_t min_vdda_mv{1620};

constexpr uint32_t max_vdda_mv{3600};
}  // namespace

static void save_before_reset() {
    micras::core::TVariablePool<1> pool;
    uint32_t                       magic = storage_magic;
    pool.add("bench/", "magic", magic, {.persist = true});

    micras::proxy::Storage storage{storage_config};
    storage.save(pool);
}

static bool restored_after_reset() {
    micras::core::TVariablePool<1> pool;
    uint32_t                       magic{};
    pool.add("bench/", "magic", magic, {.persist = true});

    micras::proxy::Storage storage{storage_config};
    return storage.restore(pool) == 1 and magic == storage_magic;
}

[[noreturn]] static void expire_watchdog() {
    const micras::proxy::Watchdog watchdog{{.timeout_ms = watchdog_timeout_ms}};

    while (true) {
        bench_done = false;
    }
}

static bool check_timer_clock() {
    MX_TIM2_Init();
    HAL_TIM_Base_Start(&htim2);

    const uint32_t start_cycles = micras::hal::Timer::get_counter();
    const uint32_t start_counts = __HAL_TIM_GET_COUNTER(&htim2);
    const uint32_t window = micras::hal::Timer::to_cycles(timer_window_us);

    while (micras::hal::Timer::get_counter() - start_cycles < window) { }

    const uint32_t cycles = micras::hal::Timer::get_counter() - start_cycles;
    const uint32_t counts = __HAL_TIM_GET_COUNTER(&htim2) - start_counts;

    RCC_ClkInitTypeDef clocks{};
    uint32_t           latency{};
    HAL_RCC_GetClockConfig(&clocks, &latency);

    const uint32_t bus_clock = HAL_RCC_GetPCLK1Freq();
    const uint32_t timer_clock = clocks.APB1CLKDivider == RCC_APB1_DIV1 ? bus_clock : 2 * bus_clock;
    const float    measured =
        static_cast<float>(counts) * static_cast<float>(SystemCoreClock) / static_cast<float>(cycles);
    const float error = measured / static_cast<float>(timer_clock) - 1.0F;

    timer_clock_error = error;
    return std::abs(error) < timer_tolerance;
}

static uint8_t software_crc(std::span<const uint8_t> data) {
    uint8_t crc = crc_initial_value;

    for (const uint8_t byte : data) {
        crc ^= byte;

        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80U) != 0 ? static_cast<uint8_t>((crc << 1U) ^ crc_polynomial) :
                                       static_cast<uint8_t>(crc << 1U);
        }
    }

    return crc;
}

static bool check_crc() {
    constexpr std::string_view check{"123456789"};
    const std::vector<uint8_t> bytes(check.begin(), check.end());

    micras::hal::Crc crc{{.handle = &hcrc}};
    const uint32_t   value = crc.calculate(bytes);

    crc_value = value;
    return value == software_crc(bytes);
}

static bool check_fmac() {
    micras::proxy::FmacFilter filter{{
        .fmac = {.init_function = MX_FMAC_Init, .handle = &hfmac},
        .filter = filter_config,
    }};

    if (not filter.was_initialized()) {
        return false;
    }

    const micras::core::ButterworthFilter::Coefficients coefficients =
        micras::core::ButterworthFilter::compute_coefficients(filter_config);
    std::array<float, 3> inputs{};
    std::array<float, 2> outputs{};
    float                largest_error{};

    for (uint32_t sample = 0; sample < 300; sample++) {
        const float input = (sample < 150 ? 0.5F : -0.25F) + 0.1F * std::sin(0.9F * static_cast<float>(sample));

        inputs = {input, inputs.at(0), inputs.at(1)};

        const float expected =
            coefficients.feed_forward.at(0) * inputs.at(0) + coefficients.feed_forward.at(1) * inputs.at(1) +
            coefficients.feed_forward.at(2) * inputs.at(2) - coefficients.feedback.at(0) * outputs.at(0) -
            coefficients.feedback.at(1) * outputs.at(1);

        outputs = {expected, outputs.at(0)};
        largest_error = std::max(largest_error, std::abs(filter.update(input) - expected));
    }

    fmac_error = largest_error;
    return largest_error < fmac_tolerance;
}

static bool check_adc() {
    micras::hal::AdcDma adc{{
        .init_function = MX_ADC3_Init,
        .handle = &hadc3,
        .max_reading = 4095,
        .reference_voltage = 3.3F,
    }};

    if (not adc.start_dma(adc_buffer, adc_snapshot)) {
        return false;
    }

    std::array<uint16_t, 2> readings{};
    const uint32_t          start = micras::hal::Timer::get_counter();
    const uint32_t          timeout = micras::hal::Timer::to_cycles(adc_timeout_us);

    while (adc.read_snapshot(readings) == 0) {
        if (micras::hal::Timer::get_counter() - start > timeout) {
            adc.stop_dma();
            return false;
        }
    }

    adc.stop_dma();

    const uint32_t vdda =
        __LL_ADC_CALC_VREFANALOG_VOLTAGE(static_cast<uint32_t>(readings.at(0)), LL_ADC_RESOLUTION_12B);

    vdda_mv = vdda;
    return vdda >= min_vdda_mv and vdda <= max_vdda_mv;
}

static void run_pin_drivers() {
    MX_GPIO_Init();

    micras::hal::Gpio       led{{.port = Status_LED_GPIO_Port, .pin = Status_LED_Pin}};
    const micras::hal::Gpio button{{.port = Button_GPIO_Port, .pin = Button_Pin}};
    led.write(button.read());

    micras::hal::Pwm pwm{
        {.init_function = MX_TIM15_Init, .handle = &htim15, .timer_channel = TIM_CHANNEL_1, .inverted = false}
    };
    pwm.set_duty_cycle(50.0F);

    micras::hal::PwmDma pwm_dma{{.init_function = MX_TIM8_Init, .handle = &htim8, .timer_channel = TIM_CHANNEL_1}};
    pwm_dma_compares.fill(static_cast<uint16_t>(pwm_dma.get_compare(50.0F)));
    pwm_dma.start_dma(pwm_dma_compares);

    const micras::hal::Encoder encoder{
        {.init_function = MX_TIM5_Init, .handle = &htim5, .timer_channel = TIM_CHANNEL_ALL}
    };
    led.write(encoder.get_counter() > 0);

    micras::hal::Spi spi{{
        .init_function = MX_SPI3_Init,
        .handle = &hspi3,
        .cs_gpio = {.port = SPI_CSn_GPIO_Port, .pin = SPI_CSn_Pin},
        .timeout = 2,
        .clock_polarity = SPI_POLARITY_HIGH,
        .clock_phase = SPI_PHASE_2EDGE,
    }};
    spi.start_transfer(spi_transmitted, spi_received);

    micras::hal::UartDma uart{{.init_function = MX_UART4_Init, .handle = &huart4}};
    uart.start_rx(uart_ring);
    uart.start_tx(spi_received);
}

int main() {
    constexpr std::array<micras::hal::Mcu::InitFunction, 2> peripheral_inits{MX_DMA_Init, MX_CRC_Init};

    micras::hal::Mcu::init({
        .clock_init = SystemClock_Config,
        .peripheral_clock_init = nullptr,
        .peripheral_inits = peripheral_inits,
        .cpu_frequency_boost = false,
    });

    if (not micras::hal::Mcu::was_reset_by_watchdog()) {
        save_before_reset();
        expire_watchdog();
    }

    watchdog_passed = true;
    storage_passed = restored_after_reset();
    timer_passed = check_timer_clock();
    crc_passed = check_crc();
    fmac_passed = check_fmac();
    adc_passed = check_adc();
    bench_done = true;

    if (start_pin_drivers) {
        run_pin_drivers();
    }

    while (true) {
        bench_done = true;
    }
}
