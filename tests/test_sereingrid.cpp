#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include "../include/SereinGrid.hpp"

namespace
{
using Grid = sereingrid::SereinGrid;

Grid make_grid()
{
    return Grid(10, 1e-6f, 1.0f);
}

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void expect_near(float actual, float expected, float tolerance, const std::string& message)
{
    expect(std::fabs(actual - expected) < tolerance, message);
}

void test_negative_coordinates()
{
    Grid grid = make_grid();

    grid.set_value(-12, -25, 15.5f);
    expect_near(grid.get_value(-12, -25), 15.5f, 1e-5f,
                "negative coordinate value was not preserved");
    expect_near(grid.get_value(-12, -24), 0.0f, 1e-5f,
                "neighboring coordinate should remain empty");
}

        void test_coordinate_conversion()
        {
            Grid grid = make_grid();
            int U, V, u, v;
            int x, y;

            grid.global_to_local(-12, -25, U, V, u, v);
            expect(U == -2 && V == -3 && u == 8 && v == 5,
                "global_to_local returned unexpected coordinates");

            grid.macro_to_global(U, V, x, y);
            expect(x == -20 && y == -30,
                "macro_to_global returned unexpected macro origin");

            grid.local_to_global(U, V, u, v, x, y);
            expect(x == -12 && y == -25,
                "local_to_global did not round-trip the coordinates");
        }

void test_add_and_pruning()
{
    Grid grid = make_grid();

    grid.set_value(5, 5, 10.0f);
    grid.add_value(5, 5, -10.0f);

    expect_near(grid.get_value(5, 5), 0.0f, 1e-6f,
                "adding the inverse value should clear the point");
}

void test_nms_suppression()
{
    Grid grid = make_grid();

    grid.set_value(0, 0, 10.0f);
    grid.set_value(1, 1, 6.0f);
    grid.set_value(40, 40, 8.0f);

    const auto maxima = grid.find_local_maxima(3.0f);

    expect(maxima.size() == 2, "NMS should return two local maxima");

    bool found_center = false, found_far = false;
    for (const auto& pt : maxima)
    {
        if (pt.first == 0 && pt.second == 0) found_center = true;
        if (pt.first == 40 && pt.second == 40) found_far = true;
    }
    expect(found_center && found_far, "NMS returned unexpected maximum coordinates");
}

void test_grid_merge()
{
    Grid target = make_grid();
    Grid source = make_grid();

    target.set_value(0, 0, 2.0f);
    source.set_value(0, 0, 3.0f);

    target.merge_from(source, 5, 5);

    expect_near(target.get_value(0, 0), 2.0f, 1e-5f,
                "merge should preserve existing target values");
    expect_near(target.get_value(5, 5), 3.0f, 1e-5f,
                "merge should apply the requested coordinate offset");
}

void test_moved_from_macro_node_can_be_reused()
{
    sereingrid::detail::SereinMacroNode source;
    source.expand(2);

    sereingrid::detail::SereinMacroNode destination = std::move(source);
    source.expand(2);
    source.set_micro_value(0, 0, 2, 1.0f);

    expect(destination.fine_grid != nullptr, "moved-to node lost its fine grid");
    expect(source.fine_grid != nullptr, "moved-from node was not reusable");

        sereingrid::detail::SereinMacroNode assigned_source;
        assigned_source.expand(2);
        sereingrid::detail::SereinMacroNode assigned_destination;
        assigned_destination = std::move(assigned_source);
        assigned_source.expand(2);
        assigned_source.set_micro_value(1, 1, 2, 2.0f);

        expect(assigned_destination.fine_grid != nullptr,
            "move assignment lost the destination fine grid");
        expect(assigned_source.fine_grid != nullptr,
            "move-assigned source was not reusable");
}
} // namespace

int main()
{
    const struct TestCase
    {
        const char* name;
        void (*run)();
    } test_cases[] = {
        {"negative coordinates", test_negative_coordinates},
        {"coordinate conversion", test_coordinate_conversion},
        {"add and pruning", test_add_and_pruning},
        {"NMS suppression", test_nms_suppression},
        {"grid merge", test_grid_merge},
        {"moved-from macro node reuse", test_moved_from_macro_node_can_be_reused},
    };

    for (const auto& test_case : test_cases)
    {
        try
        {
            test_case.run();
            std::cout << "[PASS] " << test_case.name << '\n';
        }
        catch (const std::exception& error)
        {
            std::cerr << "[FAIL] " << test_case.name << ": " << error.what() << '\n';
            return 1;
        }
    }

    std::cout << "All tests passed.\n";
    return 0;
}