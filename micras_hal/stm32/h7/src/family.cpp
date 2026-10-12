/**
 * @file
 */

#include <bit>
#include <cstdint>
#include <main.h>

#include "micras/hal/family.hpp"

namespace micras::hal::family {
namespace {
constexpr uint32_t software_lock_key{0xC5ACCE55};
}  // namespace

bool calibrate_adc(ADC_HandleTypeDef* handle) {
    return HAL_ADCEx_Calibration_Start(handle, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) == HAL_OK;
}

bool is_dma_enabled(const DMA_HandleTypeDef* handle) {
    if (IS_DMA_STREAM_INSTANCE(handle->Instance)) {
        return (static_cast<const DMA_Stream_TypeDef*>(handle->Instance)->CR & DMA_SxCR_EN) != 0;
    }

    return (static_cast<const BDMA_Channel_TypeDef*>(handle->Instance)->CCR & BDMA_CCR_EN) != 0;
}

bool has_dma_finished(const DMA_HandleTypeDef* handle) {
    if (IS_DMA_STREAM_INSTANCE(handle->Instance)) {
        return (static_cast<const DMA_Stream_TypeDef*>(handle->Instance)->CR & DMA_SxCR_EN) == 0;
    }

    const auto* const channel = static_cast<const BDMA_Channel_TypeDef*>(handle->Instance);
    return (channel->CCR & BDMA_CCR_EN) == 0 or channel->CNDTR == 0;
}

void enable_caches() {
    SCB_EnableICache();
}

bool was_reset_by_watchdog() {
    return __HAL_RCC_GET_FLAG(RCC_FLAG_IWDG1RST) != 0;
}

uint32_t reset_flags() {
    return RCC->RSR;
}

bool was_powered_on() {
    return __HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != 0;
}

void freeze_watchdog_in_debug() {
    __HAL_DBGMCU_FREEZE_IWDG1();
}

bool is_cpu_frequency_supported(bool boost) {
#ifdef FLASH_OPTSR2_CPUFREQ_BOOST
    return not boost or (FLASH->OPTSR2_CUR & FLASH_OPTSR2_CPUFREQ_BOOST) != 0;
#else
    return not boost;
#endif
}

IWDG_TypeDef* watchdog() {
    return IWDG1;
}

uint32_t timer_clock(const TIM_TypeDef* instance) {
    RCC_ClkInitTypeDef clock_config{};
    uint32_t           flash_latency{};
    HAL_RCC_GetClockConfig(&clock_config, &flash_latency);

    if (std::bit_cast<uintptr_t>(instance) >= APB2PERIPH_BASE) {
        const uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
        return clock_config.APB2CLKDivider == RCC_APB2_DIV1 ? pclk2 : 2 * pclk2;
    }

    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    return clock_config.APB1CLKDivider == RCC_APB1_DIV1 ? pclk1 : 2 * pclk1;
}

void enable_cycle_counter() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    if ((DWT->LSR & ITM_LSR_Present_Msk) != 0 and (DWT->LSR & ITM_LSR_Access_Msk) != 0) {
        DWT->LAR = software_lock_key;
    }

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
}  // namespace micras::hal::family
