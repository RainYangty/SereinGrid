#ifndef SEREIN_GRID_DETAIL_NODES_HPP
#define SEREIN_GRID_DETAIL_NODES_HPP

#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>

namespace sereingrid
{
namespace detail
{

struct SereinMicroNode
{
    float value = 0.0f;
    bool active = false;
};

struct SereinMacroNode
{
    bool is_expanded = false;
    int active_count = 0;
    float max_value = -std::numeric_limits<float>::infinity();
    int max_u = -1;
    int max_v = -1;
    std::unique_ptr<SereinMicroNode[]> fine_grid = nullptr;
    std::unordered_set<int> active_indices;

    SereinMacroNode() = default;
    SereinMacroNode(const SereinMacroNode&) = delete;
    SereinMacroNode& operator=(const SereinMacroNode&) = delete;

    SereinMacroNode(SereinMacroNode&& other) noexcept :
        is_expanded(other.is_expanded),
        active_count(other.active_count),
        max_value(other.max_value),
        max_u(other.max_u),
        max_v(other.max_v),
        fine_grid(std::move(other.fine_grid)),
        active_indices(std::move(other.active_indices))
    {
        other.reset_metadata();
    }

    SereinMacroNode& operator=(SereinMacroNode&& other) noexcept
    {
        if (this != &other) {
            is_expanded = other.is_expanded;
            active_count = other.active_count;
            max_value = other.max_value;
            max_u = other.max_u;
            max_v = other.max_v;
            fine_grid = std::move(other.fine_grid);
            active_indices = std::move(other.active_indices);
            other.reset_metadata();
        }
        return *this;
    }

    void reset_metadata()
    {
        is_expanded = false;
        active_count = 0;
        max_value = -std::numeric_limits<float>::infinity();
        max_u = -1;
        max_v = -1;
        active_indices.clear();
    }

    void expand(int N, float default_val = 0.0f)
    {
        if (is_expanded) {
            return;
        }
        fine_grid = std::make_unique<SereinMicroNode[]>(N * N);
        is_expanded = true;

        float init_val = (max_value != -std::numeric_limits<float>::infinity()) ? max_value : default_val;
        for (int i = 0; i < N * N; ++i) {
            fine_grid[i].value = init_val;
            fine_grid[i].active = false;
        }
        max_value = -std::numeric_limits<float>::infinity();
        active_count = 0;
        max_u = -1;
        max_v = -1;
        active_indices.clear();
    }

    void recompute_max_value(int N)
    {
        if (!is_expanded || !fine_grid) {
            return;
        }
        float current_max = -std::numeric_limits<float>::infinity();
        int best_u = -1, best_v = -1;

        for (int u = 0; u < N; ++u) {
            for (int v = 0; v < N; ++v) {
                int idx = u * N + v;
                if (fine_grid[idx].active && fine_grid[idx].value > current_max) {
                    current_max = fine_grid[idx].value;
                    best_u = u;
                    best_v = v;
                }
            }
        }
        max_value = current_max;
        max_u = best_u;
        max_v = best_v;
    }

    void set_micro_value(int u, int v, int N, float val, float zero_epsilon = 0.0f)
    {
        if (!is_expanded) {
            expand(N);
        }

        int index = u * N + v;
        float old_val = fine_grid[index].value;
        bool old_active = fine_grid[index].active;
        if (val <= zero_epsilon && val >= -zero_epsilon) {
            val = 0.0f;
        }

        fine_grid[index].value = val;
        fine_grid[index].active = val != 0.0f;
        if (!old_active && fine_grid[index].active) {
            ++active_count;
            active_indices.insert(index);
        }
        else if (old_active && !fine_grid[index].active) {
            --active_count;
            active_indices.erase(index);
        }

        if (!fine_grid[index].active) {
            if (old_active && old_val == max_value) {
                recompute_max_value(N);
            }
            return;
        }

        if (val > max_value) {
            max_value = val;
            max_u = u;
            max_v = v;
        }
        else if (old_active && old_val == max_value && val < old_val) {
            recompute_max_value(N);
        }
    }

    bool empty() const { return !is_expanded || !fine_grid || active_count == 0; }
};

} // namespace detail
} // namespace sereingrid

#endif // SEREIN_GRID_DETAIL_NODES_HPP
