/**
 * @file
 *
 * @brief The bench program every reference project runs, over the peripherals its board names.
 *
 * @note The bench initializes only peripherals that drive no pin, so its image is safe to flash on any
 * board with the part. It writes each result into a volatile global for the debugger or
 * STM32CubeMonitor, and ends with bench_done set.
 */

#ifndef MICRAS_LIB_REFERENCE_BENCH_HPP
#define MICRAS_LIB_REFERENCE_BENCH_HPP

#include <cstdint>
#include <main.h>

#include "micras/hal/mcu.hpp"

namespace micras::reference {
/**
 * @brief What differs between the reference projects of the families.
 */
struct Board {
    /**
     * @brief Initialization function of the UART with DMA.
     */
    hal::Mcu::InitFunction uart_init;

    /**
     * @brief Handle of the UART with DMA.
     */
    UART_HandleTypeDef* uart;

    /**
     * @brief Convert a conversion of the internal reference into the analog supply voltage.
     *
     * @note Each family reads its factory calibration of the internal reference through its own
     * low-layer ADC header.
     */
    uint32_t (*vdda_mv)(uint16_t vrefint_reading);
};

/**
 * @brief Initialize the microcontroller and run every check of the bench.
 *
 * @note The first run saves to the storage and lets the watchdog expire, the run after the reset
 * checks the reset cause and restores the storage before the other checks.
 *
 * @param board Peripherals of the reference project that differ between families.
 */
[[noreturn]] void run_bench(const Board& board);
}  // namespace micras::reference

#endif  // MICRAS_LIB_REFERENCE_BENCH_HPP
