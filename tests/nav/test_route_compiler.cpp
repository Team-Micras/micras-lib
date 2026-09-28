/**
 * @file
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numbers>
#include <vector>

#include <doctest/doctest.h>

#include "maze_reader.hpp"
#include "micras/nav/grid_pose.hpp"
#include "micras/nav/lattice.hpp"
#include "micras/nav/motion_limits.hpp"
#include "micras/nav/planner.hpp"
#include "micras/nav/route_compiler.hpp"
#include "micras/nav/segment.hpp"
#include "micras/nav/state.hpp"
#include "micras/nav/turn_table.hpp"
#include "reference_robot.hpp"

namespace micras::test {
namespace {
constexpr float tolerance{1e-3F};

const nav::LatticePose first_node{.point = {.x = 1, .y = 2}, .heading = 2};
}  // namespace

static nav::Pose end_of(const nav::Segment& segment, const nav::RunProfile& profile) {
    if (segment.kind == nav::SegmentKind::STRAIGHT) {
        return segment.start.compose({.position = {.x = segment.length, .y = 0.0F}, .orientation = 0.0F});
    }

    const nav::TurnShape& shape = reference_dynamics().get_turn(profile, segment.turn);
    const auto            end = shape.sample(shape.length());
    const float           side = segment.length < 0.0F ? -1.0F : 1.0F;

    return segment.start.compose({.position = {.x = end.x, .y = side * end.y}, .orientation = side * end.heading});
}

static bool same_pose(const nav::Pose& first, const nav::Pose& second) {
    const float heading = std::remainder(first.orientation - second.orientation, 2.0F * std::numbers::pi_v<float>);

    return std::abs(first.position.x - second.position.x) < tolerance and
           std::abs(first.position.y - second.position.y) < tolerance and std::abs(heading) < tolerance;
}

static std::vector<nav::Segment> compile(const nav::Route& route) {
    std::vector<nav::Segment> segments;
    nav::RouteCompiler::compile(route, reference_dynamics(), fast_profile, start_distance, segments);
    return segments;
}

TEST_SUITE("route_compiler") {
    TEST_CASE("drives a straight route as one straight from the start pose") {
        const nav::Route route{
            .start = first_node,
            .steps = {{.run = 3, .has_turn = false, .turn = nav::TurnId::SS90S, .side = nav::TurnSide::LEFT}},
            .stop_distance = 0.09F,
            .finish_distance = 0.0F,
            .time = 1.0F,
        };
        const float cell_size = reference_robot.maze.cell_size;

        const std::vector<nav::Segment> segments = compile(route);

        REQUIRE(segments.size() == 1);
        CHECK(segments.front().kind == nav::SegmentKind::STRAIGHT);
        CHECK(
            static_cast<double>(segments.front().length) ==
            doctest::Approx(static_cast<double>(start_distance + 3 * cell_size + 0.09F))
        );
        CHECK(same_pose(
            segments.front().start, {.position = {.x = cell_size / 2.0F, .y = cell_size - start_distance},
                                     .orientation = std::numbers::pi_v<float> / 2.0F}
        ));
    }

    TEST_CASE("places a turn between the straights of its nodes") {
        const nav::Route route{
            .start = first_node,
            .steps =
                {{.run = 1, .has_turn = true, .turn = nav::TurnId::SS90S, .side = nav::TurnSide::RIGHT},
                 {.run = 2, .has_turn = false, .turn = nav::TurnId::SS90S, .side = nav::TurnSide::LEFT}},
            .stop_distance = 0.09F,
            .finish_distance = 0.0F,
            .time = 1.0F,
        };
        const nav::TurnShape& shape = reference_dynamics().get_turn(fast_profile, nav::TurnId::SS90S);
        const float           cell_size = reference_robot.maze.cell_size;

        const std::vector<nav::Segment> segments = compile(route);

        REQUIRE(segments.size() == 3);
        CHECK(segments.at(0).kind == nav::SegmentKind::STRAIGHT);
        CHECK(segments.at(1).kind == nav::SegmentKind::TURN);
        CHECK(segments.at(2).kind == nav::SegmentKind::STRAIGHT);
        CHECK(
            static_cast<double>(segments.at(0).length) ==
            doctest::Approx(static_cast<double>(start_distance + cell_size + shape.pre))
        );
        CHECK(static_cast<double>(segments.at(1).length) == doctest::Approx(static_cast<double>(-shape.length())));
        CHECK(
            static_cast<double>(segments.at(2).length) ==
            doctest::Approx(static_cast<double>(shape.post + 2 * cell_size + 0.09F))
        );
        CHECK(same_pose(end_of(segments.at(0), fast_profile), segments.at(1).start));
        CHECK(same_pose(end_of(segments.at(1), fast_profile), segments.at(2).start));
        CHECK(same_pose(
            end_of(segments.at(2), fast_profile),
            {.position = {.x = cell_size + 2 * cell_size + 0.09F, .y = 2.5F * cell_size}, .orientation = 0.0F}
        ));
    }

    TEST_CASE("chains the segments of a planned route into the goal") {
        const MazeText   text = read_maze(std::filesystem::path{MICRAS_LIB_TEST_MAZES} / "japan2017ef.txt");
        const nav::Route route = plan_route(text, fast_profile);
        const float      cell_size = reference_robot.maze.cell_size;

        const std::vector<nav::Segment> segments = compile(route);

        std::size_t turns = 0;

        for (std::size_t index = 0; index < segments.size(); index++) {
            CAPTURE(index);
            const nav::Segment& segment = segments.at(index);

            if (segment.kind == nav::SegmentKind::TURN) {
                turns++;
            } else {
                CHECK(segment.length > 0.0F);
            }

            if (index + 1 < segments.size()) {
                CHECK(same_pose(end_of(segment, fast_profile), segments.at(index + 1).start));
            }
        }

        std::size_t route_turns = 0;

        for (const nav::RouteStep& step : route.steps) {
            route_turns += step.has_turn ? 1 : 0;
        }

        CHECK(turns == route_turns);

        const nav::Pose      stop = end_of(segments.back(), fast_profile);
        const nav::GridPoint cell{
            .x = static_cast<uint8_t>(stop.position.x / cell_size),
            .y = static_cast<uint8_t>(stop.position.y / cell_size)
        };
        bool in_goal = false;

        for (const nav::GridPoint& goal : text.goal) {
            in_goal = in_goal or goal == cell;
        }

        CHECK(in_goal);
    }
}
}  // namespace micras::test
