/**
 * @file
 */

#include <algorithm>
#include <cstdint>

#include "micras/hal/family.hpp"
#include "micras/hal/pwm.hpp"

namespace micras::hal {
/**
 * @brief Mask that turns the identifier of one of the first four channels into the position of its
 * bits in the capture and compare enable register.
 */
static constexpr uint32_t channel_shift_mask{0x1F};

/**
 * @brief Make a value written to the compare register of a channel take effect at once.
 *
 * @param handle Timer handle.
 * @param channel Timer channel.
 */
static void disable_compare_preload(TIM_HandleTypeDef* handle, uint32_t channel) {
    __HAL_TIM_DISABLE_OCxPRELOAD(handle, channel);
}

/**
 * @brief Make a value written to the compare register of a channel wait for the next update.
 *
 * @param handle Timer handle.
 * @param channel Timer channel.
 */
static void enable_compare_preload(TIM_HandleTypeDef* handle, uint32_t channel) {
    __HAL_TIM_ENABLE_OCxPRELOAD(handle, channel);
}

Pwm::Pwm(const Config& config) : handle{config.handle}, channel{config.timer_channel}, inverted{config.inverted} {
    if (this->handle->State == HAL_TIM_STATE_RESET) {
        config.init_function();
    }

    if (this->inverted) {
        this->handle->Instance->CCER |= TIM_CCER_CC1P << (this->channel & channel_shift_mask);
    }

    disable_compare_preload(this->handle, this->channel);
    this->set_duty_cycle(0.0F);
    enable_compare_preload(this->handle, this->channel);

    this->initialized = HAL_TIM_PWM_Start(this->handle, this->channel) == HAL_OK;
}

void Pwm::set_duty_cycle(float duty_cycle) {
    duty_cycle = std::clamp(duty_cycle, 0.0F, 100.0F);

    if (this->inverted) {
        duty_cycle = 100.0F - duty_cycle;
    }

    const float scaled = duty_cycle * static_cast<float>(__HAL_TIM_GET_AUTORELOAD(this->handle) + 1) * 0.01F;

    // NOLINTNEXTLINE(bugprone-incorrect-roundings)
    const auto compare = static_cast<uint32_t>(scaled + 0.5F);

    __HAL_TIM_SET_COMPARE(this->handle, this->channel, compare);
}

void Pwm::set_frequency(uint32_t frequency) {
    const uint32_t base_freq = family::timer_clock(this->handle->Instance);
    const uint32_t prescaler = this->handle->Instance->PSC;

    const uint32_t autoreload = base_freq / ((prescaler + 1) * frequency) - 1;
    __HAL_TIM_SET_AUTORELOAD(this->handle, autoreload);
    __HAL_TIM_SET_COUNTER(this->handle, 0);
}

float Pwm::get_frequency() const {
    const uint32_t autoreload = __HAL_TIM_GET_AUTORELOAD(this->handle);
    const bool     center_aligned = (this->handle->Instance->CR1 & TIM_CR1_CMS) != 0;
    const uint32_t period = center_aligned ? 2 * autoreload : autoreload + 1;
    const auto     ticks = static_cast<float>(this->handle->Instance->PSC + 1) * static_cast<float>(period);

    return static_cast<float>(family::timer_clock(this->handle->Instance)) / ticks;
}

bool Pwm::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::hal
