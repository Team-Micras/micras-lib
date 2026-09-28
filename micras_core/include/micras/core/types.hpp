/**
 * @file
 */

#ifndef MICRAS_CORE_TYPES_HPP
#define MICRAS_CORE_TYPES_HPP

#include <cstdint>

namespace micras::core {
/**
 * @brief Possible objectives of the robot.
 */
enum class Objective : uint8_t {
    EXPLORE = 0,
    RETURN = 1,
    SOLVE = 2
};
}  // namespace micras::core

#endif  // MICRAS_CORE_TYPES_HPP
