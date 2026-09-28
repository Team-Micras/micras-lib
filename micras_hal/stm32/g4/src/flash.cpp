/**
 * @file
 */

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include <main.h>

#include "micras/hal/family/flash.hpp"
#include "micras/hal/flash.hpp"

namespace micras::hal {
static constexpr uint32_t base_address{FLASH_BASE + (family::storage_first_sector * family::sector_size)};

static constexpr uint32_t erase_complete{0xFFFFFFFFU};

static_assert(FlashWord::size == sizeof(uint64_t));

static constexpr uint32_t align_size(uint32_t size) {
    return (size + FlashWord::size - 1) / FlashWord::size * FlashWord::size;
}

static bool is_storage_available() {
    return (FLASH->OPTR & FLASH_OPTR_DBANK) != 0 and FLASH_PAGE_NB == family::pages_per_bank;
}

FlashWord::FlashWord(std::span<const uint8_t> data) {
    const auto data_address = std::bit_cast<uintptr_t>(data.data());

    if (data.size() >= size and data_address % alignof(uint32_t) == 0) {
        this->source = std::bit_cast<const uint32_t*>(data.data());
        return;
    }

    const std::span<uint8_t> bytes{std::bit_cast<uint8_t*>(this->buffer.data()), size};

    std::ranges::fill(bytes, erased_value);
    std::ranges::copy(data.first(std::min<std::size_t>(data.size(), size)), bytes.begin());

    this->source = this->buffer.data();
}

const uint32_t* FlashWord::data() const {
    return this->source;
}

bool FlashWord::is_padded() const {
    return this->source == this->buffer.data();
}

std::span<const uint8_t> Flash::read(uint32_t address, uint32_t size) {
    if (address > total_size or size > total_size - address or not is_storage_available()) {
        return {};
    }

    return {std::bit_cast<const uint8_t*>(base_address + address), size};
}

std::span<const uint8_t> Flash::read(uint16_t sector, uint32_t sector_address, uint32_t size) {
    if (sector >= total_sectors or sector_address > sector_size) {
        return {};
    }

    return read(sector * sector_size + sector_address, size);
}

Flash::Status Flash::write(uint32_t address, std::span<const uint8_t> data) {
    if (address % FlashWord::size != 0) {
        return Status::MISALIGNED;
    }

    if (address > total_size or align_size(data.size()) > total_size - address) {
        return Status::OUT_OF_BOUNDS;
    }

    if (not is_storage_available() or HAL_FLASH_Unlock() != HAL_OK) {
        return Status::ERROR;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    Status status = Status::OK;

    for (uint32_t offset = 0; offset < data.size(); offset += FlashWord::size) {
        const FlashWord word{data.subspan(offset)};
        uint64_t        double_word{};
        std::memcpy(&double_word, word.data(), sizeof(double_word));

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, base_address + address + offset, double_word) != HAL_OK) {
            status = Status::ERROR;
            break;
        }
    }

    HAL_FLASH_Lock();

    return status;
}

Flash::Status Flash::write(uint16_t sector, uint32_t sector_address, std::span<const uint8_t> data) {
    if (sector >= total_sectors or sector_address > sector_size) {
        return Status::OUT_OF_BOUNDS;
    }

    return write(sector * sector_size + sector_address, data);
}

Flash::Status Flash::erase_sectors(uint16_t start_sector, uint16_t number_of_sectors) {
    if (start_sector >= total_sectors or number_of_sectors > total_sectors - start_sector) {
        return Status::OUT_OF_BOUNDS;
    }

    FLASH_EraseInitTypeDef erase_struct = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks = FLASH_BANK_2,
        .Page = start_sector,
        .NbPages = number_of_sectors,
    };

    if (not is_storage_available() or HAL_FLASH_Unlock() != HAL_OK) {
        return Status::ERROR;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    pFlash.ErrorCode = HAL_FLASH_ERROR_NONE;

    uint32_t page_error{};

    const HAL_StatusTypeDef hal_status = HAL_FLASHEx_Erase(&erase_struct, &page_error);

    HAL_FLASH_Lock();

    if (hal_status != HAL_OK or page_error != erase_complete) {
        return Status::ERROR;
    }

    return Status::OK;
}
}  // namespace micras::hal
