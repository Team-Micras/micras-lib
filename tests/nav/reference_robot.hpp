/**
 * @file
 *
 * @brief The robot the nav tests plan for: Micras v1 as the firmware describes it at MicrasFirmware
 * 4dac6e2 (config/targets/v1/robot.hpp, config/turn_margins.hpp and config/two_bend_turns.hpp).
 *
 * @note A real robot, so that the planner and the turn tables are tested on the geometry and the
 * dynamics they are tuned for. The model is defined in its own source, so that nothing built from it
 * here is a constant expression.
 */

#ifndef MICRAS_LIB_TESTS_NAV_REFERENCE_ROBOT_HPP
#define MICRAS_LIB_TESTS_NAV_REFERENCE_ROBOT_HPP

#include <array>
#include <cstdint>
#include <limits>

#include "maze_reader.hpp"
#include "micras/nav/lattice.hpp"
#include "micras/nav/motion_limits.hpp"
#include "micras/nav/planner.hpp"
#include "micras/nav/robot_model.hpp"
#include "micras/nav/turn_table.hpp"

namespace micras::test {
/**
 * @brief Physical description of the robot and of the classic maze.
 */
extern const nav::RobotModel reference_robot;

/**
 * @brief Distance kept between the outline of the robot and any obstacle, without and with the
 * risky switch.
 */
///@{
inline constexpr float turn_margin{0.015F};
inline constexpr float risky_turn_margin{0.010F};
///@}

/**
 * @brief Distance from the start pose of a run to the first wall it crosses.
 */
inline constexpr float start_distance{0.18F - (0.04F + 0.0126F / 2.0F)};

/**
 * @brief Designs of the turns of two bends, with the normal margin.
 */
inline constexpr std::array<nav::TwoBendDesign, nav::number_of_two_bend_turns> two_bend_designs{{
    {.first_angle = -0.436332315F,
     .first_curvature = 11.8644524F,
     .second_angle = 1.22173047F,
     .second_curvature = 11.8644524F,
     .pre = 0.0F},
    {.first_angle = 1.26536369F,
     .first_curvature = 17.934948F,
     .second_angle = -0.479965538F,
     .second_curvature = 17.934948F,
     .pre = 0.0F},
    {.first_angle = -0.0872664601F,
     .first_curvature = 5.19209814F,
     .second_angle = 2.44346094F,
     .second_curvature = 19.8867226F,
     .pre = 0.00999999978F},
    {.first_angle = -0.479965538F,
     .first_curvature = 17.934948F,
     .second_angle = 1.26536369F,
     .second_curvature = 17.934948F,
     .pre = 0.0F},
    {.first_angle = -1.39626336F,
     .first_curvature = 19.8867226F,
     .second_angle = 1.39626336F,
     .second_curvature = 19.8867226F,
     .pre = 0.0F},
    {.first_angle = -1.13446403F,
     .first_curvature = 13.1556044F,
     .second_angle = 0.34906584F,
     .second_curvature = 9.64987087F,
     .pre = 0.0F},
    {.first_angle = -0.741764903F,
     .first_curvature = 13.1556044F,
     .second_angle = -0.829031408F,
     .second_curvature = 13.1556044F,
     .pre = 0.0F},
    {.first_angle = -2.44346094F,
     .first_curvature = 19.8867226F,
     .second_angle = 0.0872664601F,
     .second_curvature = 4.22295713F,
     .pre = 0.0F},
}};

/**
 * @brief Designs of the turns of two bends, with the risky margin.
 */
inline constexpr std::array<nav::TwoBendDesign, nav::number_of_two_bend_turns> risky_two_bend_designs{{
    {.first_angle = -0.436332315F,
     .first_curvature = 11.8644524F,
     .second_angle = 1.22173047F,
     .second_curvature = 11.8644524F,
     .pre = 0.0F},
    {.first_angle = 1.26536369F,
     .first_curvature = 17.934948F,
     .second_angle = -0.479965538F,
     .second_curvature = 17.934948F,
     .pre = 0.0F},
    {.first_angle = -0.0872664601F,
     .first_curvature = 5.19209814F,
     .second_angle = 2.44346094F,
     .second_curvature = 19.8867226F,
     .pre = 0.00999999978F},
    {.first_angle = -0.479965538F,
     .first_curvature = 17.934948F,
     .second_angle = 1.26536369F,
     .second_curvature = 17.934948F,
     .pre = 0.0F},
    {.first_angle = -1.39626336F,
     .first_curvature = 19.8867226F,
     .second_angle = 1.39626336F,
     .second_curvature = 19.8867226F,
     .pre = 0.0F},
    {.first_angle = -1.22173047F,
     .first_curvature = 11.8644524F,
     .second_angle = 0.436332315F,
     .second_curvature = 11.8644524F,
     .pre = 0.0F},
    {.first_angle = -0.741764903F,
     .first_curvature = 9.64987087F,
     .second_angle = -0.829031408F,
     .second_curvature = 9.64987087F,
     .pre = 0.0F},
    {.first_angle = -2.44346094F,
     .first_curvature = 19.8867226F,
     .second_angle = 0.0872664601F,
     .second_curvature = 4.22295713F,
     .pre = 0.0F},
}};

/**
 * @brief Profile of a fast run with the fan on, the normal turns and the normal share of the traction.
 */
inline constexpr nav::RunProfile fast_profile{
    .racing_line = false,
    .fan = true,
    .risky = false,
    .utilization = 0.6F,
    .max_speed = std::numeric_limits<float>::infinity(),
};

/**
 * @brief Get the dynamics of the robot, with both turn tables designed when first asked for.
 *
 * @return The dynamics, shared by every test of the program.
 */
const nav::Dynamics& reference_dynamics();

/**
 * @brief Plan the fastest route of a maze whose walls are all known, from the start cell facing up.
 *
 * @note Checks on the way that the search finished, kept every label it needed, and ranked its
 * candidates by their time.
 *
 * @param maze The maze, with its goal.
 * @param profile The profile of the run.
 * @return The fastest route the search found.
 */
nav::Route plan_route(const MazeText& maze, const nav::RunProfile& profile);
}  // namespace micras::test

#endif  // MICRAS_LIB_TESTS_NAV_REFERENCE_ROBOT_HPP
