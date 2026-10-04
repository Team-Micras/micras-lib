/**
 * @file
 *
 * @brief Flash memory geometry of the STM32G4 family.
 *
 * @note The flash memory is erased in pages and programmed in double words of 64 bits; a sector of the
 * shared Flash class is a page. The geometry is the one the 512 KB parts, such as the STM32G474xE, have
 * in the dual bank mode they leave the factory in: two banks of 128 pages of 2 KB. The storage region
 * is bank 2, the upper half of the flash memory. Every operation checks the part and the mode at run
 * time and fails on any other layout.
 */

#ifndef MICRAS_HAL_FAMILY_FLASH_HPP
#define MICRAS_HAL_FAMILY_FLASH_HPP

#include <cstdint>
#include <main.h>

namespace micras::hal::family {
/**
 * @brief Number of pages of a bank of the 512 KB parts in dual bank mode.
 */
inline constexpr uint16_t pages_per_bank{128};

/**
 * @brief Number of bits of the smallest unit the flash memory programs.
 */
inline constexpr uint32_t flash_word_bits{64};

/**
 * @brief Number of bytes of an erasable page.
 */
inline constexpr uint32_t sector_size{FLASH_PAGE_SIZE};

/**
 * @brief Index of the first page reserved for data storage, counting from the start of bank 1.
 */
inline constexpr uint16_t storage_first_sector{pages_per_bank};

/**
 * @brief Number of pages reserved for data storage, the whole of bank 2.
 */
inline constexpr uint16_t storage_sectors{pages_per_bank};

/**
 * @brief Round a number of bytes up to a whole number of the double words the flash memory programs.
 *
 * @param size Number of bytes to round up.
 * @return Number of bytes the data occupies in the flash memory.
 */
constexpr uint32_t align_size(uint32_t size) {
    constexpr uint32_t word_size{flash_word_bits / 8U};
    return (size + word_size - 1) / word_size * word_size;
}

/**
 * @brief Check that the part has the layout this header describes: dual bank mode, with banks of
 * pages_per_bank pages.
 *
 * @return Whether the storage region exists on this part.
 */
inline bool is_storage_available() {
    return (FLASH->OPTR & FLASH_OPTR_DBANK) != 0 and FLASH_PAGE_NB == pages_per_bank;
}
}  // namespace micras::hal::family

#endif  // MICRAS_HAL_FAMILY_FLASH_HPP
