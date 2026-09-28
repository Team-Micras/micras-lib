/**
 * @file
 */

#include <cstdint>

#include <stm32h7xx_ll_adc.h>
#include <usart.h>

#include "bench.hpp"

static uint32_t vdda_mv(uint16_t vrefint_reading) {
    return __LL_ADC_CALC_VREFANALOG_VOLTAGE(static_cast<uint32_t>(vrefint_reading), LL_ADC_RESOLUTION_12B);
}

int main() {
    micras::reference::run_bench({
        .uart_init = MX_UART4_Init,
        .uart = &huart4,
        .vdda_mv = vdda_mv,
    });
}
