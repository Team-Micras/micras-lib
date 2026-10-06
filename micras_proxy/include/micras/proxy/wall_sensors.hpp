/**
 * @file
 */

#ifndef MICRAS_PROXY_WALL_SENSORS_HPP
#define MICRAS_PROXY_WALL_SENSORS_HPP

#include <array>
#include <cstdint>

#include "micras/core/butterworth_filter.hpp"
#include "micras/hal/adc_dma.hpp"
#include "micras/hal/pwm.hpp"
#include "micras/hal/timer_burst.hpp"

namespace micras::proxy {
/**
 * @brief Class for controlling Wall Sensors.
 *
 * @details Each sensor is an infrared emitter next to a phototransistor, and what comes out of this
 * class is the distance to whatever the emitter lights up, along its optical axis, in meters. The
 * emitter has a narrow beam that lands entirely on the wall, which scatters it in every direction,
 * so the light that comes back falls with the square of the distance. The receiver sits beside the
 * emitter, not on its axis, and only takes in light close to its own axis, so it sees the lit spot
 * at an angle that grows as the wall comes closer and loses light to its directivity:
 *
 *     reading = k * 2^(-(atan(receiver_offset / distance) / receiver_half_angle)^2) / distance^2
 *
 * One reading at a known distance therefore calibrates a sensor, by fixing k. With no receiver
 * offset this is the inverse square law, distance = reference_distance * sqrt(reference_reading /
 * reading). With one, the reading peaks at a short distance and falls again closer than that, so a
 * reading above the peak is reported as the distance of the peak, as a saturated one is.
 *
 * @details The emitters take turns, one at a time, and the receivers are read with none of them
 * lit too, so that a reading is the light of its own emitter alone with the ambient light taken
 * off, and nothing that a neighbor's emitter puts into the receiver, inside the robot or off a wall.
 * The emitter timer counts up and down and converts every receiver at both ends of its count; an
 * emitter whose output is inverted is centered on the overflow, the others on the underflow. The
 * update DMA request of the timer reloads the compare registers at every end, so that each end
 * lights one emitter or none. A frame is one end per emitter and one with none lit, which gives a
 * reading of every sensor; it is an odd number of ends, so the kind of end each position falls on
 * swaps from one frame to the next, and the order of the emitters with it, and the cycle the table
 * repeats is two frames. Nothing in the program triggers or times any of it.
 *
 * @tparam num_of_sensors Number of sensors.
 */
template <uint8_t num_of_sensors>
class TWallSensors {
public:
    /**
     * @brief Configuration struct for wall sensors.
     *
     * @note The reference reading of a sensor is what it reads at its reference distance, and is
     * what a calibration replaces. Both filters run at the rate the sensors produce values, not at
     * the rate of the control loop, so that is the sampling frequency they have to be given.
     * A reading below the noise floor, or one that works out to more than the maximum distance,
     * means nothing is within range. A reading above the maximum is saturated, and is reported as
     * the distance of the maximum reading. A wall is considered present when the slow distance is
     * below the wall distance, and absent again when it goes above it by the hysteresis. The
     * receiver offset is the distance from the optical axis of the emitter to the receiver, and the
     * receiver half angle the angle off its own axis at which the receiver's sensitivity halves. A
     * dark reading, taken with the emitter off, at or above the blind reading means that ambient
     * light saturates the receiver, which then cannot see a wall with the emitter on either. The
     * offset of a sensor is what it reads with nothing in front of it, the light its own emitter
     * and its neighbors put into the receiver without leaving the robot, and is subtracted from
     * every reading; its calibration, with the robot held up in open air, replaces it.
     */
    struct Config {
        hal::AdcDma::Config                          adc;
        std::array<hal::Pwm::Config, num_of_sensors> led_pwms;
        hal::TimerBurst::Config                      burst;
        float                                        emitter_duty_cycle;
        core::ButterworthFilter::Config              fast_filter;
        core::ButterworthFilter::Config              slow_filter;
        std::array<float, num_of_sensors>            reference_readings;
        std::array<float, num_of_sensors>            offsets;
        std::array<float, num_of_sensors>            reference_distances;
        float                                        receiver_offset;
        float                                        receiver_half_angle;
        float                                        noise_floor;
        float                                        max_reading;
        float                                        max_distance;
        float                                        blind_reading;
        float                                        wall_distance;
        float                                        wall_hysteresis;
        uint16_t                                     calibration_samples;
    };

    /**
     * @brief Distance measured by one sensor.
     *
     * @note The fast distance is for everything that is a position and the slow one for deciding
     * whether there is a wall. The reading is valid when the sensor sees anything above its noise,
     * and it is new for a single update after the sensor produces a value. The dark reading is the
     * one taken with the emitter off, as a fraction of the full scale, which is the ambient light
     * the receiver sees. The reading is blind when that light saturates the receiver, and then says
     * nothing about a wall. It is saturated when the receiver lit by its emitter reaches its
     * ceiling: the distance is then the shortest the sensor can tell, and the wall may be nearer.
     */
    struct Reading {
        float distance;
        float slow_distance;
        float dark;
        bool  valid;
        bool  saturated;
        bool  blind;
        bool  is_new;
    };

    /**
     * @brief Construct a new WallSensors object.
     *
     * @param config Configuration for the wall sensors.
     */
    explicit TWallSensors(const Config& config);

    /**
     * @brief Turn on the wall sensors IR LED.
     */
    void turn_on();

    /**
     * @brief Turn off the wall sensors IR LED.
     */
    void turn_off();

    /**
     * @brief Turn the emitter of one sensor on or off, leaving the others as they are.
     *
     * @note For measuring how much of each emitter's light reaches each receiver. A reading is the
     * difference between the scans of the two emitter groups, so with a single emitter lit every
     * receiver reads exactly the light that emitter puts into it, whichever group it belongs to.
     *
     * @param sensor_index Index of the sensor whose emitter is switched.
     * @param on Whether the emitter is lit.
     */
    void set_emitter(uint8_t sensor_index, bool on);

    /**
     * @brief Update the wall sensors readings.
     *
     * @note Nothing is recomputed unless the converter completed a sequence since the last call,
     * so this can be called faster than the sensors produce values. Restarts the converter first if
     * an error stopped it.
     */
    void update();

    /**
     * @brief Get the distance measured by a sensor.
     *
     * @param sensor_index Index of the sensor.
     * @return The reading of the sensor.
     */
    const Reading& get_reading(uint8_t sensor_index) const;

    /**
     * @brief Get the observation from a sensor.
     *
     * @param sensor_index Index of the sensor.
     * @return True if the sensor detects a wall, false otherwise.
     */
    bool get_wall(uint8_t sensor_index) const;

    /**
     * @brief Get the light a sensor receives from its emitter, with the ambient light and its offset
     * removed.
     *
     * @param sensor_index Index of the sensor.
     * @return Reading from the sensor from 0 to 1.
     */
    float get_intensity(uint8_t sensor_index) const;

    /**
     * @brief Get the reading of a sensor with its emitter off, as a fraction of the full scale.
     *
     * @note The mean of the scans at the ends of the cycle where no emitter is lit, which is the
     * ambient light the receiver sees.
     *
     * @param sensor_index Index of the sensor.
     * @return The dark reading.
     */
    float get_dark_reading(uint8_t sensor_index) const;

    /**
     * @brief Start calibrating a sensor, with the robot placed at the reference distance.
     *
     * @note The readings are averaged over the configured number of samples, which takes that many
     * updates with a new value.
     *
     * @param sensor_index Index of the sensor.
     */
    void calibrate_sensor(uint8_t sensor_index);

    /**
     * @brief Start measuring the offset of a sensor, with nothing in front of it.
     *
     * @note The readings, without the offset taken off, are averaged over the configured number of
     * samples, and their mean becomes the offset. The robot has to be held where no wall or floor
     * is within the range of the sensors.
     *
     * @param sensor_index Index of the sensor.
     */
    void calibrate_offset(uint8_t sensor_index);

    /**
     * @brief Check if a calibration is in progress.
     *
     * @return True while any sensor is still averaging.
     */
    bool is_calibrating() const;

    /**
     * @brief Get the reference reading of a sensor, which is the result of its last calibration.
     *
     * @param sensor_index Index of the sensor.
     * @return The reading at the reference distance, from 0 to 1.
     */
    float get_reference_reading(uint8_t sensor_index) const;

    /**
     * @brief Get the light an emitter puts into a receiver, with the ambient light removed.
     *
     * @note Read from the scan in which that emitter alone is lit, so it is what the reading of the
     * receiver would hold if the emitters did not take turns. The offset is not taken off. Like
     * every reading, it is the mean of the last two frames, which light the emitters in different
     * orders: the emitter lit before another disturbs its scan a little through the supply, and
     * differently in each frame, so a single frame would alternate between two values.
     *
     * @param emitter Index of the sensor whose emitter is lit.
     * @param receiver Index of the sensor whose receiver is read.
     * @return The light, as a fraction of the full scale.
     */
    float get_crosstalk(uint8_t emitter, uint8_t receiver) const;

    /**
     * @brief Replace the reference reading of a sensor, as a calibration would.
     *
     * @param sensor_index Index of the sensor.
     * @param reading The reading at the reference distance, from 0 to 1.
     */
    void set_reference_reading(uint8_t sensor_index, float reading);

    /**
     * @brief Get the offset of a sensor, which is subtracted from its readings.
     *
     * @param sensor_index Index of the sensor.
     * @return The reading with nothing in front of the sensor, from 0 to 1.
     */
    float get_offset(uint8_t sensor_index) const;

    /**
     * @brief Replace the offset of a sensor, as its calibration would.
     *
     * @param sensor_index Index of the sensor.
     * @param offset The reading with nothing in front of the sensor, from 0 to 1.
     */
    void set_offset(uint8_t sensor_index, float offset);

    /**
     * @brief Get how much the readings varied during the last calibration of a sensor.
     *
     * @note A spread that is not small means the robot moved, or something else was wrong, while
     * the sensor was being calibrated.
     *
     * @param sensor_index Index of the sensor.
     * @return The standard deviation of the readings, as a fraction of their mean.
     */
    float get_calibration_spread(uint8_t sensor_index) const;

    /**
     * @brief Get the standard deviation of the readings during the last calibration of a sensor.
     *
     * @note The spread divides it by the mean, which says nothing of a calibration whose mean is
     * close to zero, such as that of an offset.
     *
     * @param sensor_index Index of the sensor.
     * @return The standard deviation, as a fraction of the full scale.
     */
    float get_calibration_deviation(uint8_t sensor_index) const;

    /**
     * @brief Check if the ADC was initialized, its scan matches the buffer layout, and the emitters
     * run at the rate the filters were designed for.
     *
     * @note The rate of the emitters lives in the peripheral configuration and the one of the filters
     * in the constants, and nothing else would notice the day only one of them changes.
     *
     * @return True if the initialization was successful, false otherwise.
     */
    bool was_initialized() const;

private:
    /**
     * @brief Largest relative difference between the rate of the emitters and the one of the filters.
     */
    static constexpr float frequency_tolerance{0.01F};

    /**
     * @brief Number of ends of the count of the emitter timer in a frame and in a cycle, each of
     * which is a scan.
     */
    ///@{
    static constexpr uint8_t scans_per_frame{num_of_sensors + 1};
    static constexpr uint8_t scans_per_cycle{2 * scans_per_frame};
    ///@}

    /**
     * @brief Mark of an end at which no emitter is lit.
     */
    static constexpr uint8_t dark_end{0xFF};

    /**
     * @brief Assign every emitter to an end of the cycle, by the kind of end its output is centered on.
     *
     * @param config Configuration for the wall sensors.
     * @return True if half of the emitters are inverted, which the alternating ends need.
     */
    bool assign_ends(const Config& config);

    /**
     * @brief Write the compare values of every half of the cycle into the table of the timer.
     */
    void build_table();

    /**
     * @brief Start the emitter timer and the converter together, from the start of a cycle.
     *
     * @param restart Whether the converter was converting, and has to be stopped first.
     * @return True if both started.
     */
    bool synchronize(bool restart);

    /**
     * @brief Get a receiver's reading at the end of the last frame where no emitter is lit.
     *
     * @param sensor_index Index of the sensor.
     * @return The dark reading, in counts of the converter.
     */
    float get_dark_counts(uint8_t sensor_index) const;

    /**
     * @brief Get a receiver's dark reading at the time of an end of the last frame.
     *
     * @note Interpolated between the dark end of the frame before and that of the last one, which
     * come before and after every lit end of the last frame: ambient light that changes during a
     * frame, such as a lamp flickering at twice the mains frequency, then cancels to first order
     * instead of reading as light of the emitter. The first frame after a start has no frame
     * before it, and takes its own dark end.
     *
     * @param sensor_index Index of the sensor.
     * @param end End of the cycle the reading is wanted at.
     * @return The dark reading, in counts of the converter.
     */
    float get_dark_counts_at(uint8_t sensor_index, uint8_t end) const;

    /**
     * @brief Get the light an emitter put into a receiver in the last frame alone.
     *
     * @param emitter Index of the sensor whose emitter is lit.
     * @param receiver Index of the sensor whose receiver is read.
     * @return The light, as a fraction of the full scale.
     */
    float get_frame_light(uint8_t emitter, uint8_t receiver) const;

    /**
     * @brief Number of distances the shape of the reading is tabulated at.
     */
    static constexpr uint8_t shape_points{64};

    /**
     * @brief Get how the reading varies with the distance, up to the calibrated constant.
     *
     * @param distance Distance to the wall in meters.
     * @return The reading at that distance divided by k.
     */
    float shape(float distance) const;

    /**
     * @brief Get the distance a reading corresponds to.
     *
     * @param sensor_index Index of the sensor.
     * @param intensity Reading from 0 to 1, above the noise floor.
     * @return Distance in meters.
     */
    float to_distance(uint8_t sensor_index, float intensity) const;

    /**
     * @brief Averaging of the readings of one sensor during its calibration.
     */
    struct Calibration {
        float    sum;
        float    squared_sum;
        uint16_t samples_left;
        float    spread;
        float    deviation;
        bool     offset;
    };

    /**
     * @brief Get the reading of a sensor before its offset is taken off.
     *
     * @param sensor_index Index of the sensor.
     * @return The difference between its lit and dark scans, as a fraction of the full scale.
     */
    float get_raw_intensity(uint8_t sensor_index) const;

    /**
     * @brief ADC DMA handle.
     */
    hal::AdcDma adc;

    /**
     * @brief PWM handles for the infrared LEDs, one for each sensor.
     */
    std::array<hal::Pwm, num_of_sensors> led_pwms;

    /**
     * @brief Duty cycle of the emitters while the sensors are on.
     */
    float emitter_duty_cycle;

    /**
     * @brief Timer burst that reloads the compare registers of the emitters at every end.
     */
    hal::TimerBurst burst;

    /**
     * @brief Emitter lit at each end of the cycle, or dark_end.
     */
    std::array<uint8_t, scans_per_cycle> ends{};

    /**
     * @brief End of the cycle at which the emitter of each sensor is lit, and the one no emitter is
     * lit at, in each frame.
     */
    ///@{
    std::array<std::array<uint8_t, num_of_sensors>, 2> lit_end{};
    std::array<uint8_t, 2>                             dark_end_of{};
    ///@}

    /**
     * @brief Light of every emitter in every receiver, in the last frame and as the mean of the last
     * two, indexed by emitter and then by receiver.
     */
    ///@{
    std::array<std::array<float, num_of_sensors>, num_of_sensors> frame_light{};
    std::array<std::array<float, num_of_sensors>, num_of_sensors> light{};
    ///@}

    /**
     * @brief Frame the readings were last computed from, and whether the frame before it holds
     * readings too.
     */
    ///@{
    uint8_t frame{};
    bool    has_previous_frame{};
    ///@}

    /**
     * @brief Whether the emitter of each sensor takes its turn.
     */
    std::array<bool, num_of_sensors> emitting{};

    /**
     * @brief Compare values in force until the first end and from it to the second, and those the
     * update DMA request loads at every end, a row of one value per emitter for each.
     */
    ///@{
    std::array<uint32_t, num_of_sensors>                   first_row{};
    std::array<uint32_t, num_of_sensors>                   second_row{};
    std::array<uint32_t, scans_per_cycle * num_of_sensors> table{};
    ///@}

    /**
     * @brief Buffer the DMA writes to, holding one scan of every receiver per end of a cycle.
     *
     * @details Scan k is the one at end k of the cycle, the first end being the first overflow
     * after the timer starts, and the receiver of sensor i is rank i of each scan. Each half of
     * the buffer is a frame, which the converter's DMA copies to the snapshot as soon as it is full.
     *
     * @note This depends on the ADC scanning exactly num_of_sensors channels, on the emitter timer
     * being center aligned with its trigger and its DMA request on the update event, and on an
     * emitter being on for at least the settling time of the receiver before the scan starts and the
     * duration of the scan after it, while the ends stay far enough apart for the receiver to go
     * dark again. The first of those is checked by the constructor; the others live in the
     * configuration.
     */
    std::array<uint16_t, scans_per_cycle * num_of_sensors> buffer{};

    /**
     * @brief Copy of the buffer taken when a cycle completes, so that a cycle is never torn.
     */
    std::array<uint16_t, scans_per_cycle * num_of_sensors> snapshot{};

    /**
     * @brief The cycle of scans the readings are computed from.
     */
    std::array<uint16_t, scans_per_cycle * num_of_sensors> scans{};

    /**
     * @brief Number of frames completed when the readings were last computed.
     */
    uint32_t sequence{};

    /**
     * @brief Butterworth filters for the fast distances.
     */
    std::array<core::ButterworthFilter, num_of_sensors> fast_filters;

    /**
     * @brief Butterworth filters for the slow distances.
     */
    std::array<core::ButterworthFilter, num_of_sensors> slow_filters;

    /**
     * @brief Reading of each sensor at its reference distance.
     */
    std::array<float, num_of_sensors> reference_readings;

    /**
     * @brief Reading of each sensor with nothing in front of it, subtracted from its readings.
     */
    std::array<float, num_of_sensors> offsets;

    /**
     * @brief Distance each sensor was calibrated at.
     */
    std::array<float, num_of_sensors> reference_distances;

    /**
     * @brief Distance from the axis of each emitter to its receiver.
     */
    float receiver_offset;

    /**
     * @brief Angle off its axis at which the sensitivity of the receiver halves.
     */
    float receiver_half_angle;

    /**
     * @brief Distances the shape of the reading is tabulated at, from its peak to the maximum distance.
     */
    std::array<float, shape_points> shape_distances{};

    /**
     * @brief One over the square root of the shape at each tabulated distance, which grows with it.
     */
    std::array<float, shape_points> shape_scales{};

    /**
     * @brief Reading below which there is only noise.
     */
    float noise_floor;

    /**
     * @brief Distance beyond which nothing is considered within range.
     */
    float max_distance;

    /**
     * @brief Level of the receiver, lit by its emitter and the ambient light, at which it is taken as
     * saturated.
     */
    float max_reading;

    /**
     * @brief Dark reading at and above which the receiver is blinded by ambient light.
     */
    float blind_reading;

    /**
     * @brief Slow distance below which a wall is present.
     */
    float wall_distance;

    /**
     * @brief How far above the wall distance the slow distance has to go for the wall to be absent.
     */
    float wall_hysteresis;

    /**
     * @brief Number of readings averaged by a calibration.
     */
    uint16_t calibration_samples;

    /**
     * @brief Distance measured by each sensor.
     */
    std::array<Reading, num_of_sensors> readings{};

    /**
     * @brief Whether each sensor detects a wall.
     */
    std::array<bool, num_of_sensors> walls{};

    /**
     * @brief Calibration of each sensor.
     */
    std::array<Calibration, num_of_sensors> calibrations{};

    /**
     * @brief Flag to check if the ADC was initialized.
     */
    bool initialized{};
};
}  // namespace micras::proxy

#include "micras/proxy/impl/wall_sensors.tpp"  // IWYU pragma: export

#endif  // MICRAS_PROXY_WALL_SENSORS_HPP
