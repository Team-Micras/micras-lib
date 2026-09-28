/**
 * @file
 */

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "maze_reader.hpp"
#include "micras/nav/planner.hpp"
#include "reference_robot.hpp"

namespace micras::test {
namespace {
struct Golden {
    std::string_view maze;
    float            time;
    std::size_t      steps;
};

constexpr std::array<Golden, 10> golden_routes{{
    {.maze = "alljapan-033-2012-exp-fin", .time = 5.28452778F, .steps = 19},
    {.maze = "apec2016", .time = 9.0424633F, .steps = 26},
    {.maze = "apec2017", .time = 7.20180178F, .steps = 24},
    {.maze = "apec2018", .time = 5.90276146F, .steps = 17},
    {.maze = "apec2019", .time = 7.93852806F, .steps = 34},
    {.maze = "japan2013ef", .time = 7.11609554F, .steps = 27},
    {.maze = "japan2017ef", .time = 7.34178638F, .steps = 51},
    {.maze = "maze1", .time = 3.97810149F, .steps = 21},
    {.maze = "maze2", .time = 3.71197319F, .steps = 10},
    {.maze = "uk2016f", .time = 4.59111643F, .steps = 23},
}};
}  // namespace

TEST_SUITE("planner") {
    TEST_CASE("finds the golden route of every contest maze") {
        for (const Golden& golden : golden_routes) {
            CAPTURE(golden.maze);
            const MazeText text =
                read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / (std::string{golden.maze} + ".txt"));
            const nav::Route route = plan_route(text, fast_profile);

            CHECK(route.time == doctest::Approx(golden.time).epsilon(1e-6));
            CHECK(route.steps.size() == golden.steps);
        }
    }

    TEST_CASE("drives no slower once walls are taken out of the maze") {
        const MazeText text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "maze2.txt");
        MazeText       opened = text;

        for (std::size_t index = 0; index < opened.walls.size(); index += 16) {
            opened.walls.at(index).second = false;
        }

        CHECK(plan_route(opened, fast_profile).time <= plan_route(text, fast_profile).time);
    }

    TEST_CASE("drives faster with the fan than without") {
        const MazeText  text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "maze1.txt");
        nav::RunProfile no_fan = fast_profile;
        no_fan.fan = false;

        CHECK(plan_route(text, fast_profile).time < plan_route(text, no_fan).time);
    }
}
}  // namespace micras::test
