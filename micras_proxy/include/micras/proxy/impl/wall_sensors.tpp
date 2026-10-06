/**
 * @file
 */

#ifndef MICRAS_PROXY_WALL_SENSORS_TPP
#define MICRAS_PROXY_WALL_SENSORS_TPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <span>

#include "micras/core/butterworth_filter.hpp"
#include "micras/core/utils.hpp"
#include "micras/hal/pwm.hpp"

namespace micras::proxy {
template <uint8_t num_of_sensors>
TWallSensors<num_of_sensors>::TWallSensors(const Config& config) :
    adc{config.adc},
    led_pwms{core::make_array<hal::Pwm>(config.led_pwms)},
    emitter_duty_cycle{config.emitter_duty_cycle},
    burst{config.burst},
    fast_filters{core::make_array<core::ButterworthFilter, num_of_sensors>(config.fast_filter)},
    slow_filters{core::make_array<core::ButterworthFilter, num_of_sensors>(config.slow_filter)},
    reference_readings{config.reference_readings},
    offsets{config.offsets},
    reference_distances{config.reference_distances},
    receiver_offset{config.receiver_offset},
    receiver_half_angle{config.receiver_half_angle},
    noise_floor{config.noise_floor},
    max_distance{config.max_distance},
    max_reading{config.max_reading},
    blind_reading{config.blind_reading},
    wall_distance{config.wall_distance},
    wall_hysteresis{config.wall_hysteresis},
    calibration_samples{config.calibration_samples} {
    if (this->receiver_offset > 0.0F) {
        const float step = this->max_distance / static_cast<float>(4 * shape_points);
        float       peak = step;

        for (float distance = step; distance < this->max_distance; distance += step) {
            if (this->shape(distance) > this->shape(peak)) {
                peak = distance;
            }
        }

        const float ratio = std::pow(this->max_distance / peak, 1.0F / static_cast<float>(shape_points - 1));
        float       distance = peak;

        for (uint8_t i = 0; i < shape_points; i++) {
            this->shape_distances.at(i) = distance;
            this->shape_scales.at(i) = 1.0F / std::sqrt(this->shape(distance));
            distance *= ratio;
        }
    }

    const bool  balanced = this->assign_ends(config);
    const bool  started = this->synchronize(false);
    const float frame_frequency = 2.0F * this->led_pwms.front().get_frequency() / static_cast<float>(scans_per_frame);

    this->initialized =
        balanced and started and this->adc.was_initialized() and this->burst.was_initialized() and
        config.adc.handle->Init.NbrOfConversion == num_of_sensors and
        std::abs(frame_frequency - config.fast_filter.sampling_frequency) <
            frequency_tolerance * config.fast_filter.sampling_frequency and
        std::ranges::all_of(this->led_pwms, [](const hal::Pwm& led_pwm) { return led_pwm.was_initialized(); });
}

template <uint8_t num_of_sensors>
bool TWallSensors<num_of_sensors>::assign_ends(const Config& config) {
    std::array<uint8_t, num_of_sensors> peaks{};
    std::array<uint8_t, num_of_sensors> troughs{};
    uint8_t                             peak_count = 0;
    uint8_t                             trough_count = 0;

    for (uint8_t i = 0; i < num_of_sensors; i++) {
        if (config.led_pwms.at(i).inverted) {
            peaks.at(peak_count++) = i;
        } else {
            troughs.at(trough_count++) = i;
        }
    }

    if (peak_count != trough_count) {
        return false;
    }

    for (uint8_t frame_index = 0; frame_index < 2; frame_index++) {
        uint8_t next_peak = 0;
        uint8_t next_trough = 0;
        uint8_t darks = 0;

        for (uint8_t position = 0; position < scans_per_frame; position++) {
            const auto end = static_cast<uint8_t>(frame_index * scans_per_frame + position);
            const bool overflow = end % 2 == 0;
            uint8_t    sensor = dark_end;

            if (overflow and next_peak < peak_count) {
                sensor = peaks.at(next_peak++);
            } else if (not overflow and next_trough < trough_count) {
                sensor = troughs.at(next_trough++);
            }

            this->ends.at(end) = sensor;

            if (sensor == dark_end) {
                this->dark_end_of.at(frame_index) = end;
                darks++;
            } else {
                this->lit_end.at(frame_index).at(sensor) = end;
            }
        }

        if (darks != 1) {
            return false;
        }
    }

    return true;
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::build_table() {
    const auto row = [this](uint8_t half, std::span<uint32_t> values) {
        const uint8_t before = this->ends.at(static_cast<std::size_t>((half + scans_per_cycle - 1) % scans_per_cycle));
        const uint8_t after = this->ends.at(static_cast<std::size_t>(half % scans_per_cycle));

        for (uint8_t i = 0; i < num_of_sensors; i++) {
            const bool lit = this->emitting.at(i) and (before == i or after == i);
            values[i] = this->led_pwms.at(i).get_compare(lit ? this->emitter_duty_cycle : 0.0F);
        }
    };

    row(0, this->first_row);
    row(1, this->second_row);

    for (uint8_t end = 0; end < scans_per_cycle; end++) {
        row(static_cast<uint8_t>(end + 2),
            std::span{this->table}.subspan(static_cast<std::size_t>(end * num_of_sensors), num_of_sensors));
    }
}

template <uint8_t num_of_sensors>
bool TWallSensors<num_of_sensors>::synchronize(bool restart) {
    this->has_previous_frame = false;
    this->build_table();

    const bool armed = this->burst.arm(this->first_row, this->second_row, this->table);

    if (restart) {
        this->adc.stop_dma();
    }

    const bool converting = this->adc.start_dma(this->buffer, this->snapshot, true);

    this->burst.start();
    return armed and converting;
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_dark_counts_at(uint8_t sensor_index, uint8_t end) const {
    const float current = this->get_dark_counts(sensor_index);

    if (not this->has_previous_frame) {
        return current;
    }

    const uint8_t current_end = this->dark_end_of.at(this->frame);
    const auto    previous = static_cast<float>(
        this->scans.at(static_cast<std::size_t>(this->dark_end_of.at(1 - this->frame) * num_of_sensors + sensor_index))
    );
    const float ends_before = static_cast<float>(current_end - end) / static_cast<float>(scans_per_frame);

    return current - (current - previous) * ends_before;
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_dark_counts(uint8_t sensor_index) const {
    return static_cast<float>(
        this->scans.at(static_cast<std::size_t>(this->dark_end_of.at(this->frame) * num_of_sensors + sensor_index))
    );
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::shape(float distance) const {
    const float angle = std::atan(this->receiver_offset / distance) / this->receiver_half_angle;
    return std::exp2(-angle * angle) / (distance * distance);
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::to_distance(uint8_t sensor_index, float intensity) const {
    const float reference_distance = this->reference_distances.at(sensor_index);
    const float ratio = this->reference_readings.at(sensor_index) / intensity;

    if (this->receiver_offset <= 0.0F) {
        return reference_distance * std::sqrt(ratio);
    }

    const float scale = std::sqrt(ratio) / std::sqrt(this->shape(reference_distance));

    if (scale <= this->shape_scales.front()) {
        return this->shape_distances.front();
    }

    if (scale >= this->shape_scales.back()) {
        return this->max_distance;
    }

    const auto  upper = std::ranges::upper_bound(this->shape_scales, scale);
    const auto  index = static_cast<uint8_t>(std::distance(this->shape_scales.begin(), upper));
    const float low = this->shape_scales.at(index - 1);
    const float fraction = (scale - low) / (this->shape_scales.at(index) - low);

    return this->shape_distances.at(index - 1) +
           fraction * (this->shape_distances.at(index) - this->shape_distances.at(index - 1));
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::turn_on() {
    this->emitting.fill(true);
    this->build_table();
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::turn_off() {
    this->emitting.fill(false);
    this->build_table();
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::set_emitter(uint8_t sensor_index, bool on) {
    this->emitting.at(sensor_index) = on;
    this->build_table();
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::update() {
    const uint32_t restarts = hal::AdcDma::get_restarts();

    this->adc.recover();

    if (hal::AdcDma::get_restarts() != restarts) {
        this->synchronize(true);
    }

    const uint32_t current_sequence = this->adc.read_snapshot(this->scans, this->frame);
    const bool     is_new = current_sequence != this->sequence;

    this->sequence = current_sequence;

    for (uint8_t i = 0; i < num_of_sensors; i++) {
        Reading& reading = this->readings.at(i);
        reading.is_new = is_new;

        if (not is_new) {
            continue;
        }

        const float intensity = this->get_intensity(i);

        const float distance = intensity >= this->noise_floor ?
                                   this->to_distance(i, std::min(intensity, this->max_reading)) :
                                   this->max_distance;

        reading.valid = distance < this->max_distance;
        reading.dark = this->get_dark_reading(i);
        reading.blind = reading.dark >= this->blind_reading;
        reading.distance = this->fast_filters.at(i).update(std::min(distance, this->max_distance));
        reading.slow_distance = this->slow_filters.at(i).update(std::min(distance, this->max_distance));

        if (not reading.valid or reading.slow_distance > this->wall_distance + this->wall_hysteresis) {
            this->walls.at(i) = false;
        } else if (reading.slow_distance < this->wall_distance) {
            this->walls.at(i) = true;
        }

        Calibration& calibration = this->calibrations.at(i);

        if (calibration.samples_left == 0) {
            continue;
        }

        const float sample = calibration.offset ? this->get_raw_intensity(i) : intensity;

        calibration.sum += sample;
        calibration.squared_sum += sample * sample;
        calibration.samples_left--;

        if (calibration.samples_left == 0) {
            const float mean = calibration.sum / static_cast<float>(this->calibration_samples);
            const float variance =
                calibration.squared_sum / static_cast<float>(this->calibration_samples) - mean * mean;

            calibration.deviation = std::sqrt(std::max(variance, 0.0F));
            calibration.spread = mean > 0.0F ? calibration.deviation / mean : 0.0F;

            if (calibration.offset) {
                this->offsets.at(i) = mean;
            } else {
                this->reference_readings.at(i) = std::max(mean, this->noise_floor);
            }
        }
    }

    if (is_new) {
        this->has_previous_frame = true;
    }
}

template <uint8_t num_of_sensors>
const typename TWallSensors<num_of_sensors>::Reading&
    TWallSensors<num_of_sensors>::get_reading(uint8_t sensor_index) const {
    return this->readings.at(sensor_index);
}

template <uint8_t num_of_sensors>
bool TWallSensors<num_of_sensors>::get_wall(uint8_t sensor_index) const {
    return this->walls.at(sensor_index);
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_intensity(uint8_t sensor_index) const {
    return std::max(this->get_raw_intensity(sensor_index) - this->offsets.at(sensor_index), 0.0F);
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_raw_intensity(uint8_t sensor_index) const {
    return this->get_crosstalk(sensor_index, sensor_index);
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_crosstalk(uint8_t emitter, uint8_t receiver) const {
    const uint8_t end = this->lit_end.at(this->frame).at(emitter);
    const auto    lit = static_cast<float>(this->scans.at(static_cast<std::size_t>(end * num_of_sensors + receiver)));

    return (lit - this->get_dark_counts_at(receiver, end)) / this->adc.get_max_reading();
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_dark_reading(uint8_t sensor_index) const {
    return this->get_dark_counts(sensor_index) / this->adc.get_max_reading();
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::calibrate_sensor(uint8_t sensor_index) {
    this->calibrations.at(sensor_index) = {
        .sum = 0.0F,
        .squared_sum = 0.0F,
        .samples_left = this->calibration_samples,
        .spread = 0.0F,
        .deviation = 0.0F,
        .offset = false,
    };
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::calibrate_offset(uint8_t sensor_index) {
    this->calibrations.at(sensor_index) = {
        .sum = 0.0F,
        .squared_sum = 0.0F,
        .samples_left = this->calibration_samples,
        .spread = 0.0F,
        .deviation = 0.0F,
        .offset = true,
    };
}

template <uint8_t num_of_sensors>
bool TWallSensors<num_of_sensors>::is_calibrating() const {
    return std::ranges::any_of(this->calibrations, [](const Calibration& calibration) {
        return calibration.samples_left > 0;
    });
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_reference_reading(uint8_t sensor_index) const {
    return this->reference_readings.at(sensor_index);
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::set_reference_reading(uint8_t sensor_index, float reading) {
    this->reference_readings.at(sensor_index) = reading;
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_offset(uint8_t sensor_index) const {
    return this->offsets.at(sensor_index);
}

template <uint8_t num_of_sensors>
void TWallSensors<num_of_sensors>::set_offset(uint8_t sensor_index, float offset) {
    this->offsets.at(sensor_index) = offset;
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_calibration_spread(uint8_t sensor_index) const {
    return this->calibrations.at(sensor_index).spread;
}

template <uint8_t num_of_sensors>
float TWallSensors<num_of_sensors>::get_calibration_deviation(uint8_t sensor_index) const {
    return this->calibrations.at(sensor_index).deviation;
}

template <uint8_t num_of_sensors>
bool TWallSensors<num_of_sensors>::was_initialized() const {
    return this->initialized;
}
}  // namespace micras::proxy

#endif  // MICRAS_PROXY_WALL_SENSORS_TPP
