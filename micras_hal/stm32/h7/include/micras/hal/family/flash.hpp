/**
 * @file
 *
 * @brief Flash memory geometry of the STM32H7 family.
 *
 * @note The flash memory is erased in sectors and programmed in flash words of 256 bits. The storage
 * region is the upper half of bank 1: on the STM32H725xG, sectors 4 to 7 of 128 KB.
 */

#ifndef MICRAS_HAL_FAMILY_FLASH_HPP
#define MICRAS_HAL_FAMILY_FLASH_HPP

#include <cstdint>
#include <main.h>

namespace micras::hal::family {
/**
 * @brief Number of bits of the smallest unit the flash memory programs.
 */
inline constexpr uint32_t flash_word_bits{FLASH_NB_32BITWORD_IN_FLASHWORD * 32U};

/**
 * @brief Number of bytes of an erasable sector.
 */
inline constexpr uint32_t sector_size{FLASH_SECTOR_SIZE};

/**
 * @brief Index of the first sector reserved for data storage.
 */
inline constexpr uint16_t storage_first_sector{FLASH_SECTOR_TOTAL / 2};

/**
 * @brief Number of sectors reserved for data storage.
 */
inline constexpr uint16_t storage_sectors{FLASH_SECTOR_TOTAL / 2};
}  // namespace micras::hal::family

#endif  // MICRAS_HAL_FAMILY_FLASH_HPP
