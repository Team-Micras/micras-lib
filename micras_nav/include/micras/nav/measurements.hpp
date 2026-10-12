/**
 * @file
 */

#ifndef MICRAS_NAV_MEASUREMENTS_HPP
#define MICRAS_NAV_MEASUREMENTS_HPP

#include <array>

#include "micras/core/vector.hpp"
#include "micras/nav/robot_model.hpp"

namespace micras::nav {
/**
 * @brief Reading of one wall sensor.
 *
 * @details The distance is measured along the optical axis, in meters. It is valid when the receiver
 * sees anything above its noise, so a reading that is not valid means that nothing is within range.
 *
 * A saturated reading is one whose receiver is at its ceiling, lit by more than it can measure. Its
 * distance is then the shortest one the sensor can tell, so the wall is at most that far, and may be
 * nearer.
 *
 * A blind reading is one whose receiver ambient light saturates, which says nothing about a wall.
 *
 * @note The new flag is set for a single iteration after the sensor produces a value. An estimator
 * that is fed the same sample twice counts it as two independent observations and becomes
 * overconfident, so that is who needs it.
 */
struct WallReading {
    float distance;
    bool  valid;
    bool  saturated;
    bool  blind;
    bool  is_new;
};

/**
 * @brief Everything the navigation needs from the hardware, sampled once per iteration.
 *
 * @details The navigation takes plain data instead of holding devices, so it depends on no proxy and
 * runs anywhere this struct can be filled: on the robot, in a simulator or in a test. The wheel
 * angles are cumulative, in radians, positive when the robot moves forward. The angular rate is the
 * raw one around the vertical axis, in rad/s, positive counterclockwise, with the bias still in it.
 * The acceleration is in the body frame, x forward and y to the left, in m/s^2.
 */
struct Measurements {
    float                                           left_wheel_angle;
    float                                           right_wheel_angle;
    float                                           angular_rate;
    core::Vector                                    acceleration;
    bool                                            imu_is_new;
    std::array<WallReading, number_of_wall_sensors> walls;
};
}  // namespace micras::nav

#endif  // MICRAS_NAV_MEASUREMENTS_HPP
