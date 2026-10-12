/**
 * @file
 */

#include <cstdint>
#include <memory>
#include <numbers>

#include <doctest/doctest.h>

#include "maze_reader.hpp"
#include "micras/nav/grid_pose.hpp"
#include "micras/nav/maze.hpp"
#include "micras/nav/motion_limits.hpp"
#include "micras/nav/planner.hpp"
#include "micras/nav/robot_model.hpp"
#include "micras/nav/turn_table.hpp"
#include "reference_robot.hpp"

namespace micras::test {
const nav::RobotModel reference_robot{
    .maze = {.cell_size = 0.18F, .wall_thickness = 0.0126F, .wall_minnaert = 1.0F},
    .chassis =
        {
            .mass = 0.07F,
            .yaw_inertia = 2.9e-5F,
            .wheel_radius = 0.011F,
            .rolling_compliance = 56e-6F,
            .track_width = 0.04575F,
            .half_width = 0.0251F,
            .front_length = 0.0549F,
            .rear_length = 0.0365F,
        },
    .traction =
        {
            .friction_coefficient = 1.0F,
            .fan_downforce = 3.0F,
            .fan_offset = 0.0175F,
            .lateral_compliance = 0.0048F,
        },
    .drive =
        {
            .torque_constant = 0.00336F,
            .resistance = 12.62F,
            .gear_ratio = 5.25F,
            .supply_voltage = 19.63F,
            .static_friction_voltage = 0.09F,
        },
    .wall_sensors = {{
        {.position = {.x = 0.0439F, .y = 0.0215F}, .angle = 0.0F, .half_angle = 0.09F},
        {.position = {.x = 0.0526F, .y = 0.0153F}, .angle = std::numbers::pi_v<float> / 4.0F, .half_angle = 0.09F},
        {.position = {.x = 0.0526F, .y = -0.0153F}, .angle = -std::numbers::pi_v<float> / 4.0F, .half_angle = 0.09F},
        {.position = {.x = 0.0439F, .y = -0.0215F}, .angle = 0.0F, .half_angle = 0.09F},
    }},
    .noise =
        {
            .gyroscope = 6.5e-5F,
            .gyroscope_bias_walk = 1.0e-4F,
            .wheel_angle = 2.0F * std::numbers::pi_v<float> / 16384.0F,
            .longitudinal_slip = 0.005F,
            .longitudinal_slip_per_acceleration = 0.0005F,
            .lateral_slip = 0.003F,
            .wall_range = 0.001F,
            .wall_range_per_meter = 0.017F,
        },
    .gyroscope_scale = 1.0F,
};

const nav::Dynamics& reference_dynamics() {
    static const nav::Dynamics dynamics{{
        .model = reference_robot,
        .turns =
            nav::TurnTable{
                reference_robot, turn_margin, nav::TurnTable::place(reference_robot, turn_margin, two_bend_designs)
            },
        .risky_turns =
            nav::TurnTable{
                reference_robot, risky_turn_margin,
                nav::TurnTable::place(reference_robot, risky_turn_margin, risky_two_bend_designs)
            },
        .max_linear_speed = 3.0F,
        .max_angular_speed = 12.0F,
        .voltage_reserve = 0.15F,
    }};

    return dynamics;
}

nav::Route plan_route(const MazeText& maze, const nav::RunProfile& profile) {
    using Planner = nav::TPlanner<16, 16>;

    nav::TMaze<16, 16> map{{.start = {.position = maze.start, .orientation = nav::Side::UP}, .goal = maze.goal}};
    fill_maze(maze, map);

    const auto planner =
        std::make_unique<Planner>(reference_dynamics(), Planner::Config{.start_distance = start_distance});
    planner->begin(map, nav::WallAssumption::PESSIMISTIC, profile);

    while (not planner->step(1000)) { }

    CHECK(planner->is_finished());
    CHECK(planner->is_exact());
    REQUIRE(planner->get_number_of_routes() > 0);

    nav::Route route;
    planner->get_route(0, route);

    for (uint8_t index = 1; index < planner->get_number_of_routes(); index++) {
        nav::Route candidate;
        planner->get_route(index, candidate);
        CHECK(candidate.time >= route.time);
    }

    return route;
}
}  // namespace micras::test
