#ifndef SEREIN_GRID_SEREIN_GRID_INL
#define SEREIN_GRID_SEREIN_GRID_INL

#define SEREIN_GRID_IMPLEMENTATION_FILE
#include "SereinGrid.hpp"
#include <cmath>
#include <stdexcept>

namespace sereingrid
{

inline bool SereinGrid::is_zero(float value) const
{
    return std::fabs(value) <= zero_epsilon;
}

inline SereinGrid::SereinGrid(int expansion_scale, float zero_epsilon, float physical_spacing) :
    N(expansion_scale),
    spacing(physical_spacing),
    zero_epsilon(zero_epsilon)
{
    if (N <= 0 || spacing <= 0.0f || zero_epsilon < 0.0f) {
        throw std::invalid_argument("Invalid grid scale, physical spacing, or zero epsilon");
    }
}

inline int SereinGrid::get_scale() const
{
    return N;
}

inline float SereinGrid::get_spacing() const
{
    return spacing;
}

inline float SereinGrid::get_zero_epsilon() const
{
    return zero_epsilon;
}

inline void SereinGrid::global_to_local(int x, int y, int& U, int& V, int& u, int& v) const
{
    detail::global_to_local(N, x, y, U, V, u, v);
}

inline void SereinGrid::macro_to_global(int U, int V, int& x, int& y) const
{
    detail::macro_to_global(N, U, V, x, y);
}

inline void SereinGrid::local_to_global(int U, int V, int u, int v, int& x, int& y) const
{
    detail::local_to_global(N, U, V, u, v, x, y);
}

inline void SereinGrid::set_value(int x, int y, float value)
{
    int U, V, u, v;
    global_to_local(x, y, U, V, u, v);
    uint64_t key = get_key(U, V);
    if (is_zero(value)) {
        auto it = space.find(key);
        if (it == space.end()) {
            return;
        }
        it->second.set_micro_value(u, v, N, value, zero_epsilon);
        if (it->second.empty()) {
            space.erase(it);
        }
        return;
    }

    space[key].set_micro_value(u, v, N, value, zero_epsilon);
}

inline void SereinGrid::add_value(int x, int y, float delta)
{
    int U, V, u, v;
    global_to_local(x, y, U, V, u, v);

    uint64_t key = get_key(U, V);
    detail::SereinMacroNode& macro = space[key];
    if (!macro.is_expanded) {
        macro.expand(N);
    }

    int index = u * N + v;
    float new_val = macro.fine_grid[index].value + delta;
    macro.set_micro_value(u, v, N, new_val, zero_epsilon);
    if (macro.empty()) {
        space.erase(key);
    }
}

inline void SereinGrid::move_point(int src_x, int src_y, int dx, int dy)
{
    float val = get_value(src_x, src_y);
    if (is_zero(val)) {
        return;
    }
    set_value(src_x, src_y, 0.0f);
    add_value(src_x + dx, src_y + dy, val);
}

inline float SereinGrid::get_value(int x, int y) const
{
    int U, V, u, v;
    global_to_local(x, y, U, V, u, v);

    auto it = space.find(get_key(U, V));
    if (it == space.end() || !it->second.is_expanded) {
        return 0.0f;
    }
    const detail::SereinMicroNode& node = it->second.fine_grid[u * N + v];
    return node.active ? node.value : 0.0f;
}

inline bool SereinGrid::is_local_maximum(int x, int y, float R_phys) const
{
    float current_val = get_value(x, y);
    if (current_val <= 0.0f) {
        return false;
    }

    int center_U, center_V, center_u, center_v;
    global_to_local(x, y, center_U, center_V, center_u, center_v);
    double radius_index = static_cast<double>(R_phys) * N / spacing;
    int macro_radius = static_cast<int>(std::ceil(radius_index / N)) + 1;
    double R_sq = radius_index * radius_index;

    for (int dU = -macro_radius; dU <= macro_radius; ++dU) {
        for (int dV = -macro_radius; dV <= macro_radius; ++dV) {
            int target_U = center_U + dU;
            int target_V = center_V + dV;
            auto it = space.find(get_key(target_U, target_V));
            if (it == space.end() || !it->second.is_expanded) {
                continue;
            }

            const detail::SereinMacroNode& target_macro = it->second;
            if (target_macro.max_value < current_val) {
                continue;
            }

            if (target_macro.max_u >= 0 && target_macro.max_v >= 0) {
                int peak_x = target_U * N + target_macro.max_u;
                int peak_y = target_V * N + target_macro.max_v;
                if (peak_x != x || peak_y != y) {
                    double dx = static_cast<double>(peak_x - x);
                    double dy = static_cast<double>(peak_y - y);
                    double peak_dist_sq = dx * dx + dx * dy + dy * dy;
                    if (target_macro.max_value > current_val && peak_dist_sq <= R_sq) {
                        return false;
                    }
                }
            }

            for (int u = 0; u < N; ++u) {
                for (int v = 0; v < N; ++v) {
                    if (u == target_macro.max_u && v == target_macro.max_v) {
                        continue;
                    }
                    int nx = target_U * N + u;
                    int ny = target_V * N + v;
                    if (nx == x && ny == y) {
                        continue;
                    }
                    const detail::SereinMicroNode& neighbor = target_macro.fine_grid[u * N + v];
                    if (!neighbor.active || neighbor.value <= current_val) {
                        continue;
                    }
                    double dx = static_cast<double>(nx - x);
                    double dy = static_cast<double>(ny - y);
                    if (dx * dx + dx * dy + dy * dy <= R_sq) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

inline std::vector<std::pair<int, int>> SereinGrid::find_local_maxima(float R_phys) const
{
    std::vector<std::pair<int, int>> maxima;
    for (const auto& entry : space) {
        const detail::SereinMacroNode& macro = entry.second;
        if (!macro.is_expanded || !macro.fine_grid) {
            continue;
        }

        int32_t U = static_cast<int32_t>(entry.first >> 32);
        int32_t V = static_cast<int32_t>(entry.first & 0xFFFFFFFFu);
        for (int u = 0; u < N; ++u) {
            for (int v = 0; v < N; ++v) {
                const detail::SereinMicroNode& node = macro.fine_grid[u * N + v];
                if (node.active && node.value > 0.0f) {
                    int x = U * N + u;
                    int y = V * N + v;
                    if (is_local_maximum(x, y, R_phys)) {
                        maxima.emplace_back(x, y);
                    }
                }
            }
        }
    }
    return maxima;
}

inline void SereinGrid::merge_from(const SereinGrid& other, int offset_x, int offset_y)
{
    if (N != other.N || spacing != other.spacing) {
        throw std::invalid_argument("SereinGrid scales and physical spacing must match");
    }

    for (const auto& entry : other.space) {
        const auto& other_macro = entry.second;
        if (!other_macro.is_expanded) {
            continue;
        }

        int32_t U = static_cast<int32_t>(entry.first >> 32);
        int32_t V = static_cast<int32_t>(entry.first & 0xFFFFFFFFu);
        for (int u = 0; u < N; ++u) {
            for (int v = 0; v < N; ++v) {
                const detail::SereinMicroNode& node = other_macro.fine_grid[u * N + v];
                if (node.active) {
                    int src_x = U * N + u;
                    int src_y = V * N + v;
                    add_value(src_x + offset_x, src_y + offset_y, node.value);
                }
            }
        }
    }
}

inline void SereinGrid::clear()
{
    space.clear();
}

} // namespace sereingrid

#endif // SEREIN_GRID_SEREIN_GRID_INL
