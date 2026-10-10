/**
 * @file
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

#include "micras/hal/adc_dma.hpp"
#include "micras/hal/family.hpp"

extern "C" {
/**
 * @brief Callback of the vendor HAL for the end of a DMA transfer of a converter.
 *
 * @param hadc Handle of the converter.
 */
// NOLINTNEXTLINE(readability-identifier-naming) the name is fixed by the vendor HAL
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    micras::hal::AdcDma::on_sequence_complete(hadc);
}

/**
 * @brief Callback of the vendor HAL for an overrun of a converter or a transfer error of its DMA.
 *
 * @param hadc Handle of the converter.
 */
// NOLINTNEXTLINE(readability-identifier-naming) the name is fixed by the vendor HAL
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
    micras::hal::AdcDma::on_half_complete(hadc);
}

// NOLINTNEXTLINE(readability-identifier-naming) the name is fixed by the vendor HAL
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc) {
    micras::hal::AdcDma::on_error(hadc);
}
}

namespace micras::hal {
std::array<AdcDma*, AdcDma::max_instances> AdcDma::instances{};
uint32_t                                   AdcDma::restarts{};

AdcDma::AdcDma(const Config& config) :
    max_reading{config.max_reading}, reference_voltage{config.reference_voltage}, handle{config.handle} {
    if (this->handle->State == HAL_ADC_STATE_RESET) {
        config.init_function();
    }

    const bool calibrated = family::calibrate_adc(this->handle);

    auto* const slot = std::ranges::find(instances, nullptr);

    if (slot == instances.end()) {
        return;
    }

    *slot = this;
    this->initialized = calibrated;
}

AdcDma::~AdcDma() {
    std::ranges::replace(instances, this, static_cast<AdcDma*>(nullptr));
}

bool AdcDma::start_dma(std::span<uint32_t> buffer) {
    this->transfer = buffer;

    if (HAL_ADC_Start_DMA(this->handle, buffer.data(), buffer.size()) != HAL_OK) {
        this->initialized = false;
        return false;
    }

    return true;
}

bool AdcDma::start_dma(std::span<uint16_t> buffer) {
    return this->start_dma({std::bit_cast<uint32_t*>(buffer.data()), buffer.size()});
}

bool AdcDma::start_dma(std::span<uint16_t> buffer, std::span<uint16_t> snapshot, bool halves) {
    if (snapshot.size() != buffer.size() or (halves and buffer.size() % 2 != 0)) {
        this->initialized = false;
        return false;
    }

    this->buffer = buffer;
    this->snapshot = snapshot;
    this->halves = halves;
    this->last_half = 1;

    return this->start_dma(buffer);
}

uint32_t AdcDma::read_snapshot(std::span<uint16_t> destination, uint8_t& half) const {
    while (true) {
        const uint32_t before = this->sequence;
        half = this->last_half;
        const uint32_t after = this->read_snapshot(destination);

        if (after == before) {
            return after;
        }
    }
}

uint32_t AdcDma::read_snapshot(std::span<uint16_t> destination) const {
    const std::span<const uint16_t> source = this->snapshot.first(std::min(this->snapshot.size(), destination.size()));
    uint32_t                        before = this->sequence;

    while (true) {
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::ranges::copy(source, destination.begin());
        std::atomic_signal_fence(std::memory_order_seq_cst);

        const uint32_t after = this->sequence;

        if (after == before) {
            return after;
        }

        before = after;
    }
}

void AdcDma::on_sequence_complete(const ADC_HandleTypeDef* handle) {
    AdcDma* const instance = find(handle);

    if (instance == nullptr or instance->stopped) {
        return;
    }

    if (__HAL_ADC_GET_FLAG(handle, ADC_FLAG_OVR)) {
        instance->stopped = true;
        return;
    }

    if (instance->halves) {
        const std::size_t middle = instance->buffer.size() / 2;
        std::ranges::copy(instance->buffer.subspan(middle), instance->snapshot.subspan(middle).begin());
    } else {
        std::ranges::copy(instance->buffer, instance->snapshot.begin());
    }

    instance->last_half = 1;
    instance->sequence = instance->sequence + 1;
}

void AdcDma::on_half_complete(const ADC_HandleTypeDef* handle) {
    AdcDma* const instance = find(handle);

    if (instance == nullptr or instance->stopped or not instance->halves) {
        return;
    }

    if (__HAL_ADC_GET_FLAG(handle, ADC_FLAG_OVR)) {
        instance->stopped = true;
        return;
    }

    std::ranges::copy(instance->buffer.first(instance->buffer.size() / 2), instance->snapshot.begin());
    instance->last_half = 0;
    instance->sequence = instance->sequence + 1;
}

void AdcDma::on_error(const ADC_HandleTypeDef* handle) {
    AdcDma* const instance = find(handle);

    if (instance != nullptr) {
        instance->stopped = true;
    }
}

AdcDma* AdcDma::find(const ADC_HandleTypeDef* handle) {
    const auto* const found = std::ranges::find_if(instances, [handle](const AdcDma* instance) {
        return instance != nullptr and instance->handle == handle;
    });

    return found == instances.end() ? nullptr : *found;
}

void AdcDma::stop_dma() {
    HAL_ADC_Stop_DMA(this->handle);
}

void AdcDma::recover() {
    const DMA_HandleTypeDef* const dma = this->handle->DMA_Handle;
    const bool frozen = dma->State == HAL_DMA_STATE_BUSY and
                        (__HAL_ADC_GET_FLAG(this->handle, ADC_FLAG_OVR) or not family::is_dma_enabled(dma));

    if (not this->stopped and not frozen) {
        return;
    }

    HAL_ADC_Stop_DMA(this->handle);
    this->handle->State = this->handle->State & ~(HAL_ADC_STATE_ERROR_DMA | HAL_ADC_STATE_ERROR_INTERNAL);
    this->stopped = false;

    if (HAL_ADC_Start_DMA(this->handle, this->transfer.data(), this->transfer.size()) != HAL_OK) {
        this->stopped = true;
    }

    restarts++;
}

uint32_t AdcDma::get_restarts() {
    return restarts;
}

uint16_t AdcDma::get_max_reading() const {
    return this->max_reading;
}

float AdcDma::get_reference_voltage() const {
    return this->reference_voltage;
}

bool AdcDma::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::hal
