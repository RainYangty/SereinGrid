#ifndef SEREIN_GRID_SEREIN_GRID_HPP
#define SEREIN_GRID_SEREIN_GRID_HPP

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "detail/Coordinates.hpp"
#include "detail/Nodes.hpp"

namespace sereingrid
{

class SereinGrid
{
private:
    int N;
    float spacing;
    float zero_epsilon;
    std::unordered_map<uint64_t, detail::SereinMacroNode> space;

    inline uint64_t get_key(int U, int V) const { return detail::make_key(U, V); }

    inline bool is_zero(float value) const;

public:
    explicit SereinGrid(int expansion_scale = 10, float zero_epsilon = 1e-6f, float physical_spacing = 1.0f);

    int get_scale() const;
    float get_spacing() const;
    float get_zero_epsilon() const;

    void global_to_local(int x, int y, int& U, int& V, int& u, int& v) const;
    void macro_to_global(int U, int V, int& x, int& y) const;
    void local_to_global(int U, int V, int u, int v, int& x, int& y) const;

    void set_value(int x, int y, float value);
    void add_value(int x, int y, float delta);
    void move_point(int src_x, int src_y, int dx, int dy);
    float get_value(int x, int y) const;
    bool is_local_maximum(int x, int y, float R_phys) const;
    std::vector<std::pair<int, int>> find_local_maxima(float R_phys) const;
    void merge_from(const SereinGrid& other, int offset_x = 0, int offset_y = 0);
    void clear();
};

} // namespace sereingrid

#ifndef SEREIN_GRID_IMPLEMENTATION_FILE
#include "SereinGrid.inl"
#endif

#endif // SEREIN_GRID_SEREIN_GRID_HPP
