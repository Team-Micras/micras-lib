/**
 * @file
 */

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <doctest/doctest.h>

#include "micras/nav/lattice.hpp"
#include "micras/nav/motion_limits.hpp"
#include "micras/nav/state.hpp"
#include "micras/nav/turn_table.hpp"
#include "reference_robot.hpp"

namespace micras::test {
TEST_SUITE("turn_table") {
    TEST_CASE("fits every turn in the maze with the normal margin") {
        const nav::TurnTable table{
            reference_robot, turn_margin, nav::TurnTable::place(reference_robot, turn_margin, two_bend_designs)
        };

        CHECK(table.is_valid());
    }

    TEST_CASE("fits every turn in the maze with the risky margin") {
        const nav::TurnTable table{
            reference_robot, risky_turn_margin,
            nav::TurnTable::place(reference_robot, risky_turn_margin, risky_two_bend_designs)
        };

        CHECK(table.is_valid());
    }

    TEST_CASE("clears the walls with every turn of two bends") {
        CHECK(nav::TurnTable::clears(reference_robot, turn_margin, two_bend_designs));
        CHECK(nav::TurnTable::clears(reference_robot, risky_turn_margin, risky_two_bend_designs));
    }

    TEST_CASE("rejects the designs of a smaller margin with a larger one") {
        CHECK_FALSE(nav::TurnTable::clears(reference_robot, 0.03F, risky_two_bend_designs));
    }

    TEST_CASE("rejects a turn nothing fits") {
        std::array<nav::TwoBendDesign, nav::number_of_two_bend_turns> designs = two_bend_designs;
        designs.front().first_curvature = 0.0F;

        CHECK_FALSE(nav::TurnTable::clears(reference_robot, turn_margin, designs));

        const nav::TurnTable table{
            reference_robot, turn_margin, nav::TurnTable::place(reference_robot, turn_margin, designs)
        };
        CHECK_FALSE(table.is_valid());
    }

    TEST_CASE("ends every turn on the exit node of its primitive") {
        const float cell_size = reference_robot.maze.cell_size;

        for (const bool risky : {false, true}) {
            nav::RunProfile profile = fast_profile;
            profile.risky = risky;

            for (uint8_t index = 0; index < nav::number_of_turns; index++) {
                const auto turn = static_cast<nav::TurnId>(index);
                CAPTURE(risky);
                CAPTURE(index);

                const nav::TurnShape&  shape = reference_dynamics().get_turn(profile, turn);
                const nav::LatticePose entry{
                    .point = {.x = 2, .y = 1},
                    .heading = static_cast<uint8_t>(nav::get_primitive(turn).diagonal_entry ? 1 : 0),
                };
                const nav::LatticePose exit = nav::get_turn_exit(entry, turn, nav::TurnSide::LEFT);
                const nav::Pose        expected = exit.to_pose(cell_size);

                const auto      end = shape.sample(shape.length());
                const nav::Pose curve_start =
                    entry.to_pose(cell_size).compose({.position = {.x = shape.pre, .y = 0.0F}, .orientation = 0.0F});
                const nav::Pose curve_end =
                    curve_start.compose({.position = {.x = end.x, .y = end.y}, .orientation = end.heading});
                const nav::Pose reached =
                    curve_end.compose({.position = {.x = shape.post, .y = 0.0F}, .orientation = 0.0F});

                CHECK(shape.valid);
                CHECK(std::abs(reached.position.x - expected.position.x) < 1e-3F);
                CHECK(std::abs(reached.position.y - expected.position.y) < 1e-3F);
                CHECK(
                    std::abs(
                        std::remainder(reached.orientation - expected.orientation, 2.0F * std::numbers::pi_v<float>)
                    ) < 1e-3F
                );
            }
        }
    }
}
}  // namespace micras::test
