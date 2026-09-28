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
