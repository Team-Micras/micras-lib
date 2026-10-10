/**
 * @file
 */

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "micras/hal/timer_burst.hpp"

namespace micras::hal {
namespace {
/**
 * @brief Offset of the first compare register from the start of the timer, in words.
 */
constexpr uint32_t first_compare_offset{offsetof(TIM_TypeDef, CCR1) / sizeof(uint32_t)};
}  // namespace

TimerBurst::TimerBurst(const Config& config) : handle{config.handle} {
    if (this->handle->State == HAL_TIM_STATE_RESET) {
        config.init_function();
    }

    this->initialized = this->handle->hdma[TIM_DMA_ID_UPDATE] != nullptr;
}

bool TimerBurst::arm(
    std::span<const uint32_t> first, std::span<const uint32_t> second, std::span<const uint32_t> table
) {
    if (not this->initialized or first.empty() or second.size() != first.size() or table.size() % first.size() != 0) {
        return false;
    }

    TIM_TypeDef* const       timer = this->handle->Instance;
    DMA_HandleTypeDef* const dma = this->handle->hdma[TIM_DMA_ID_UPDATE];

    this->stop();

    const auto load = [timer](std::span<const uint32_t> values) {
        const std::array<volatile uint32_t*, 4> compares{&timer->CCR1, &timer->CCR2, &timer->CCR3, &timer->CCR4};
        std::size_t                             channel = 0;

        for (const uint32_t value : values) {
            if (channel == compares.size()) {
                break;
            }

            *compares.at(channel) = value;
            channel++;
        }
    };

    load(first);
    timer->EGR = TIM_EGR_UG;
    timer->SR = ~TIM_SR_UIF;
    load(second);
    timer->DCR = static_cast<uint32_t>(first.size() - 1) << TIM_DCR_DBL_Pos | first_compare_offset << TIM_DCR_DBA_Pos;

    if (HAL_DMA_Start(
            dma, std::bit_cast<uint32_t>(table.data()), std::bit_cast<uint32_t>(&timer->DMAR), table.size()
        ) != HAL_OK) {
        return false;
    }

    timer->DIER |= TIM_DIER_UDE;
    return true;
}

void TimerBurst::start() {
    this->handle->Instance->CR1 |= TIM_CR1_CEN;
}

void TimerBurst::stop() {
    TIM_TypeDef* const timer = this->handle->Instance;

    timer->CR1 &= ~TIM_CR1_CEN;
    timer->DIER &= ~TIM_DIER_UDE;
    HAL_DMA_Abort(this->handle->hdma[TIM_DMA_ID_UPDATE]);
    timer->CNT = 0;
}

bool TimerBurst::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::hal
