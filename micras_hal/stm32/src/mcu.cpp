/**
 * @file
 */

#include <algorithm>
#include <cstdint>
#include <span>

#include <main.h>
#include "micras/hal/family.hpp"
#include "micras/hal/gpio.hpp"
#include "micras/hal/mcu.hpp"
#include "micras/hal/pwm.hpp"
#include "micras/hal/timer.hpp"

namespace micras::hal {
/**
 * @brief Key values of the independent watchdog key register.
 */
///@{
static constexpr uint32_t watchdog_key_reload{0xAAAA};
static constexpr uint32_t watchdog_key_write{0x5555};
static constexpr uint32_t watchdog_key_start{0xCCCC};
///@}

/**
 * @brief Smallest divider the watchdog prescaler can apply to the low speed oscillator.
 */
static constexpr uint32_t watchdog_min_divider{4};

/**
 * @brief Largest value the watchdog prescaler register can hold.
 */
static constexpr uint32_t watchdog_max_prescaler{6};

/**
 * @brief Largest value the 12 bit watchdog reload register can hold.
 */
static constexpr uint32_t watchdog_max_reload{0xFFF};

/**
 * @brief Time to wait for the watchdog registers to take effect before giving up.
 *
 * @note Bounded so that a low speed oscillator that never starts cannot hang the boot.
 */
static constexpr uint32_t watchdog_timeout_us{1000};

/**
 * @brief Compare value that keeps an inverted PWM output inactive, above any period.
 */
static constexpr uint32_t inactive_compare{0xFFFFFFFF};

bool     Mcu::watchdog_reset{};
bool     Mcu::cpu_frequency_supported{};
uint32_t Mcu::reset_flags{};
uint32_t Mcu::previous_trace{};

/**
 * @brief Mark left by set_trace, in memory the startup code neither loads nor clears.
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) it outlives the program on purpose
__attribute__((noinit)) static volatile uint32_t trace;

void Mcu::init(const Config& config) {
    watchdog_reset = family::was_reset_by_watchdog();
    reset_flags = family::reset_flags();
    previous_trace = family::was_powered_on() ? 0 : trace;
    trace = 0;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    family::enable_caches();

    HAL_Init();

    family::freeze_watchdog_in_debug();

    cpu_frequency_supported = family::is_cpu_frequency_supported(config.cpu_frequency_boost);

    config.clock_init();

    if (config.peripheral_clock_init != nullptr) {
        config.peripheral_clock_init();
    }

    Timer::init();

    for (const InitFunction init_function : config.peripheral_inits) {
        init_function();
    }
}

void Mcu::emergency_stop(std::span<const Pwm::Config> pwm_outputs, std::span<const Gpio::Config> enable_gpios) {
    for (const auto& pwm_output : pwm_outputs) {
        const uint32_t compare = pwm_output.inverted ? inactive_compare : 0;
        __HAL_TIM_SET_COMPARE(pwm_output.handle, pwm_output.timer_channel, compare);
    }

    for (const auto& enable_gpio : enable_gpios) {
        HAL_GPIO_WritePin(enable_gpio.port, enable_gpio.pin, GPIO_PIN_RESET);
    }
}

void Mcu::set_watchdog_timeout(uint32_t timeout_ms) {
    uint32_t prescaler = 0;
    uint32_t ticks = timeout_ms * LSI_VALUE / (watchdog_min_divider * 1000);

    while (ticks > watchdog_max_reload + 1 and prescaler < watchdog_max_prescaler) {
        prescaler++;
        ticks /= 2;
    }

    const uint32_t reload = std::clamp<uint32_t>(ticks, 1, watchdog_max_reload + 1) - 1;

    IWDG_TypeDef* const watchdog = family::watchdog();

    watchdog->KR = watchdog_key_start;
    watchdog->KR = watchdog_key_write;
    watchdog->PR = prescaler;
    watchdog->RLR = reload;

    const uint32_t start = Timer::get_counter();
    const uint32_t limit = Timer::to_cycles(watchdog_timeout_us);

    while ((watchdog->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0) {
        if (Timer::get_counter() - start > limit) {
            break;
        }
    }

    refresh_watchdog();
}

void Mcu::refresh_watchdog() {
    family::watchdog()->KR = watchdog_key_reload;
}

bool Mcu::was_reset_by_watchdog() {
    return watchdog_reset;
}

bool Mcu::is_cpu_frequency_supported() {
    return cpu_frequency_supported;
}

uint32_t Mcu::get_reset_flags() {
    return reset_flags;
}

void Mcu::set_trace(uint32_t mark) {
    trace = mark;
}

uint32_t Mcu::get_previous_trace() {
    return previous_trace;
}
}  // namespace micras::hal
