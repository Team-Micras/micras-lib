/**
 * @file
 */

#include <bit>
#include <cstdint>
#include <main.h>

#include "micras/hal/family.hpp"

namespace micras::hal::family {
bool calibrate_adc(ADC_HandleTypeDef* handle) {
    return HAL_ADCEx_Calibration_Start(handle, ADC_SINGLE_ENDED) == HAL_OK;
}

bool is_dma_enabled(const DMA_HandleTypeDef* handle) {
    return (handle->Instance->CCR & DMA_CCR_EN) != 0;
}

bool has_dma_finished(const DMA_HandleTypeDef* handle) {
    return (handle->Instance->CCR & DMA_CCR_EN) == 0 or handle->Instance->CNDTR == 0;
}

void enable_caches() { }

bool was_reset_by_watchdog() {
    return __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != 0;
}

void freeze_watchdog_in_debug() {
    __HAL_DBGMCU_FREEZE_IWDG();
}

bool is_cpu_frequency_supported(bool /*boost*/) {
    return true;
}

IWDG_TypeDef* watchdog() {
    return IWDG;
}

uint32_t timer_clock(const TIM_TypeDef* instance) {
    RCC_ClkInitTypeDef clock_config{};
    uint32_t           flash_latency{};
    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

    if (std::bit_cast<uintptr_t>(instance) >= APB2PERIPH_BASE) {
        const uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
        return clock_config.APB2CLKDivider == RCC_HCLK_DIV1 ? pclk2 : 2 * pclk2;
    }

    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    return clock_config.APB1CLKDivider == RCC_HCLK_DIV1 ? pclk1 : 2 * pclk1;
}

void enable_cycle_counter() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
}  // namespace micras::hal::family
