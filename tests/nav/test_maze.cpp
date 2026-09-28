/**
 * @file
 */

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include "maze_reader.hpp"
#include "micras/nav/grid_pose.hpp"
#include "micras/nav/maze.hpp"

namespace micras::test {
namespace {
using Maze = nav::TMaze<16, 16>;

constexpr nav::GridPose maze_start{.position = {.x = 0, .y = 0}, .orientation = nav::Side::UP};

constexpr std::array<nav::GridPoint, 4> maze_goal{
    {{.x = 8, .y = 8}, {.x = 7, .y = 8}, {.x = 8, .y = 7}, {.x = 7, .y = 7}}
};
}  // namespace

static void check_same_walls(const Maze& first, const Maze& second) {
    for (uint8_t row = 0; row < 16; row++) {
        for (uint8_t column = 0; column < 16; column++) {
            for (const nav::Side side : nav::all_sides) {
                const nav::GridPose wall{.position = {.x = column, .y = row}, .orientation = side};
                CAPTURE(column);
                CAPTURE(row);
                CAPTURE(static_cast<int>(side));
                REQUIRE(first.get_wall(wall) == second.get_wall(wall));
            }
        }
    }
}

TEST_SUITE("maze") {
    TEST_CASE("reads the contest mazes with their start and goal") {
        for (const auto& entry : std::filesystem::directory_iterator{MICRAS_LIB_TEST_MAZES}) {
            CAPTURE(entry.path().filename().string());
            const MazeText text = read_maze(entry.path());

            CHECK(text.width == 16);
            CHECK(text.height == 16);
            CHECK(text.walls.size() == 2 * 16 * 16 + 2 * 16);
            CHECK(text.start == maze_start.position);
            CHECK(text.goal.size() == 4);
        }
    }

    TEST_CASE("round trips a whole maze through its serialization") {
        const MazeText text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "maze1.txt");
        Maze           maze{{.start = maze_start, .goal = maze_goal}};
        fill_maze(text, maze);

        const std::vector<uint8_t> bytes = maze.serialize();
        CHECK(bytes.size() == 3 + 16 * 16 / 2);
        CHECK(bytes.at(1) == 16);
        CHECK(bytes.at(2) == 16);

        Maze copy{{.start = maze_start, .goal = maze_goal}};
        copy.deserialize(bytes.data(), static_cast<uint16_t>(bytes.size()));

        check_same_walls(maze, copy);
        CHECK(copy.get_cost(maze_start.position) == maze.get_cost(maze_start.position));
    }

    TEST_CASE("keeps the unknown walls unknown through the serialization") {
        const MazeText text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "japan2017ef.txt");
        Maze           maze{{.start = maze_start, .goal = maze_goal}};

        for (std::size_t index = 0; index < text.walls.size(); index += 3) {
            maze.set_wall(text.walls.at(index).first, text.walls.at(index).second);
        }

        const std::vector<uint8_t> bytes = maze.serialize();
        Maze                       copy{{.start = maze_start, .goal = maze_goal}};
        copy.deserialize(bytes.data(), static_cast<uint16_t>(bytes.size()));

        check_same_walls(maze, copy);
    }

    TEST_CASE("ignores a serialization of another format or size") {
        const MazeText text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "maze1.txt");
        Maze           maze{{.start = maze_start, .goal = maze_goal}};
        fill_maze(text, maze);

        const Maze fresh{{.start = maze_start, .goal = maze_goal}};
        Maze       copy{{.start = maze_start, .goal = maze_goal}};

        std::vector<uint8_t> bytes = maze.serialize();
        bytes.at(0) ^= 0xFFU;
        copy.deserialize(bytes.data(), static_cast<uint16_t>(bytes.size()));
        check_same_walls(copy, fresh);

        bytes = maze.serialize();
        copy.deserialize(bytes.data(), static_cast<uint16_t>(bytes.size() - 1));
        check_same_walls(copy, fresh);
    }

    TEST_CASE("floods the cost to the goal through the known walls") {
        const MazeText text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "maze1.txt");
        Maze           maze{{.start = maze_start, .goal = maze_goal}};
        fill_maze(text, maze);

        for (const nav::GridPoint& cell : maze_goal) {
            CHECK(maze.get_cost(cell) == 0);
        }

        nav::GridPose pose = maze_start;
        uint16_t      steps = 0;

        while (not maze.is_goal(pose.position) and steps < 256) {
            const std::optional<nav::GridPose> next = maze.get_next(pose);

            if (not next.has_value()) {
                FAIL_CHECK("no neighbor leads to the goal");
                break;
            }

            CHECK(maze.get_cost(next->position) + 1 == maze.get_cost(pose.position));
            pose = *next;
            steps++;
        }

        CHECK(maze.is_goal(pose.position));
        CHECK(steps == maze.get_cost(maze_start.position));
    }
}
}  // namespace micras::test
