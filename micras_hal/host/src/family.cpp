/**
 * @file
 */

#include <cstdint>
#include <main.h>

#include "micras/hal/family.hpp"

namespace micras::hal::family {
bool calibrate_adc(ADC_HandleTypeDef* /*handle*/) {
    return true;
}

bool is_dma_enabled(const DMA_HandleTypeDef* /*handle*/) {
    return true;
}

bool has_dma_finished(const DMA_HandleTypeDef* /*handle*/) {
    return false;
}

void enable_caches() { }

bool was_reset_by_watchdog() {
    return false;
}

void freeze_watchdog_in_debug() { }

bool is_cpu_frequency_supported(bool /*boost*/) {
    return true;
}

IWDG_TypeDef* watchdog() {
    static IWDG_TypeDef registers{};
    return &registers;
}

uint32_t timer_clock(const TIM_TypeDef* instance) {
    return instance->kernel_clock;
}

void enable_cycle_counter() { }
}  // namespace micras::hal::family
