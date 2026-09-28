/**
 * @file
 *
 * @brief Reader of the text mazes the simulator draws and the nav tests plan on.
 */

#ifndef MICRAS_LIB_TESTS_MAZE_READER_HPP
#define MICRAS_LIB_TESTS_MAZE_READER_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "micras/nav/grid_pose.hpp"
#include "micras/nav/maze.hpp"

namespace micras::test {
/**
 * @brief The walls of a maze, with its start and its goal.
 *
 * @note The text has a line of posts and horizontal walls (`o---o`) above and below every row of
 * cells, and a line of vertical walls (`|`) through every row, the top line first. A cell is four
 * characters wide, with an `S` in the start cell and a `G` in every goal cell.
 */
struct MazeText {
    /**
     * @brief Size of the maze in cells.
     */
    ///@{
    uint8_t width{};
    uint8_t height{};
    ///@}

    /**
     * @brief Every wall of the maze, once: the right and upper wall of every cell, and the left and
     * lower walls of the border, each with whether it is present.
     */
    std::vector<std::pair<nav::GridPose, bool>> walls;

    /**
     * @brief Cell marked as the start.
     */
    nav::GridPoint start{};

    /**
     * @brief Cells marked as the goal.
     */
    std::vector<nav::GridPoint> goal;
};

/**
 * @brief Read a maze from its text.
 *
 * @param path Path of the text file.
 * @return The maze.
 */
inline MazeText read_maze(const std::filesystem::path& path) {
    std::ifstream            file{path};
    std::vector<std::string> lines;

    for (std::string line; std::getline(file, line);) {
        lines.push_back(line);
    }

    if (lines.size() < 3 or lines.size() % 2 == 0 or lines.front().size() < 5 or lines.front().size() % 4 != 1) {
        throw std::runtime_error{"not a maze: " + path.string()};
    }

    MazeText maze{
        .width = static_cast<uint8_t>(lines.front().size() / 4),
        .height = static_cast<uint8_t>(lines.size() / 2),
        .walls = {},
        .start = {},
        .goal = {},
    };

    const auto character = [&lines](std::size_t row, std::size_t column) {
        const std::string& line = lines.at(row);
        return column < line.size() ? line.at(column) : ' ';
    };

    for (uint8_t y = 0; y < maze.height; y++) {
        const std::size_t row = 2 * static_cast<std::size_t>(maze.height - 1 - y) + 1;

        for (uint8_t x = 0; x < maze.width; x++) {
            const std::size_t    column = 4 * static_cast<std::size_t>(x);
            const nav::GridPoint cell{.x = x, .y = y};

            maze.walls.emplace_back(
                nav::GridPose{.position = cell, .orientation = nav::Side::RIGHT}, character(row, column + 4) == '|'
            );
            maze.walls.emplace_back(
                nav::GridPose{.position = cell, .orientation = nav::Side::UP}, character(row - 1, column + 2) == '-'
            );

            if (x == 0) {
                maze.walls.emplace_back(
                    nav::GridPose{.position = cell, .orientation = nav::Side::LEFT}, character(row, column) == '|'
                );
            }

            if (y == 0) {
                maze.walls.emplace_back(
                    nav::GridPose{.position = cell, .orientation = nav::Side::DOWN},
                    character(row + 1, column + 2) == '-'
                );
            }

            const char mark = character(row, column + 2);

            if (mark == 'S') {
                maze.start = cell;
            } else if (mark == 'G') {
                maze.goal.push_back(cell);
            }
        }
    }

    return maze;
}

/**
 * @brief Make every wall of a maze known to a map of the same size.
 *
 * @tparam width Width of the maze in cells.
 * @tparam height Height of the maze in cells.
 * @param text The maze read from its text.
 * @param maze The map to fill.
 */
template <uint8_t width, uint8_t height>
void fill_maze(const MazeText& text, nav::TMaze<width, height>& maze) {
    for (const auto& [wall, present] : text.walls) {
        maze.set_wall(wall, present);
    }

    maze.flood(maze.get_goal());
}
}  // namespace micras::test

#endif  // MICRAS_LIB_TESTS_MAZE_READER_HPP
