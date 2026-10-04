/**
 * @file
 *
 * @brief The steps of the shared HAL classes that differ between chip families.
 *
 * @note Internal to micras_hal: each backend implements these functions once, the stm32 backend in the
 * sources of the family CMake selects and the host backend in its own. Nothing here names a chip, so
 * the shared classes call them without any conditional compilation.
 */

#ifndef MICRAS_HAL_FAMILY_HPP
#define MICRAS_HAL_FAMILY_HPP

#include <cstdint>
#include <main.h>

namespace micras::hal::family {
/**
 * @brief Calibrate a converter for single-ended conversions.
 *
 * @param handle Handle of the converter, initialized and not enabled.
 * @return True if the calibration succeeded, false otherwise.
 */
bool calibrate_adc(ADC_HandleTypeDef* handle);

/**
 * @brief Check whether the stream or channel of a DMA is enabled.
 *
 * @note A transfer error disables the stream or channel in hardware, so a transfer the vendor HAL
 * still holds as busy and whose stream or channel is disabled has stopped, whether or not its
 * interrupt is enabled to report it.
 *
 * @param handle Handle of the DMA, initialized.
 * @return True if the stream or channel is enabled, false otherwise.
 */
bool is_dma_enabled(const DMA_HandleTypeDef* handle);

/**
 * @brief Check whether a transfer of a DMA in normal mode is over.
 *
 * @note A stream of the DMA controllers of the STM32H7 clears its enable bit once it moved its last
 * item, while a channel of its BDMA or of the STM32G4 keeps it set and only its counter reaches zero.
 * A transfer error disables either, which ends the transfer as well.
 *
 * @param handle Handle of the DMA, running a transfer started in normal mode.
 * @return True if the DMA moved its last item or an error stopped it, false while items are left.
 */
bool has_dma_finished(const DMA_HandleTypeDef* handle);

/**
 * @brief Enable the caches the family runs its code from, before the vendor HAL is initialized.
 */
void enable_caches();

/**
 * @brief Read from the reset flags whether the last reset came from the independent watchdog.
 *
 * @note The flags are read before they are cleared, so this is called once at startup.
 *
 * @return True if the independent watchdog reset the microcontroller, false otherwise.
 */
bool was_reset_by_watchdog();

/**
 * @brief Stop the independent watchdog while the core is halted by a debugger.
 */
void freeze_watchdog_in_debug();

/**
 * @brief Check whether the part can run at the core frequency the board asks for.
 *
 * @param boost Whether the board runs the core above the frequency the part allows by default.
 * @return True if the part supports the requested frequency, false otherwise.
 */
bool is_cpu_frequency_supported(bool boost);

/**
 * @brief Get the registers of the independent watchdog.
 *
 * @return Registers of the independent watchdog.
 */
IWDG_TypeDef* watchdog();

/**
 * @brief Get the frequency of the clock that feeds a timer.
 *
 * @param instance Timer peripheral.
 * @return Timer clock frequency in Hz.
 */
uint32_t timer_clock(const TIM_TypeDef* instance);

/**
 * @brief Start the cycle counter of the core from zero.
 */
void enable_cycle_counter();
}  // namespace micras::hal::family

#endif  // MICRAS_HAL_FAMILY_HPP
