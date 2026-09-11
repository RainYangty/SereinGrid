#ifndef SEREIN_GRID_HPP
#define SEREIN_GRID_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sereingrid {

namespace detail {

// ==================== 基础数据节点定义 ====================

struct SereinMicroNode
{
	float value = 0.0f;	 // 用于 NMS 比较的特征值/置信度
	bool active = false; // 是否被激活/赋值
};

struct SereinMacroNode
{
	bool is_expanded = false;
	int active_count = 0;
	float max_value = -std::numeric_limits<float>::infinity();

	// 保存最大值的微观局部坐标
	int max_u = -1;
	int max_v = -1;

	std::unique_ptr<SereinMicroNode[]> fine_grid = nullptr; // 指向 N x N 连续内存

	SereinMacroNode() = default;

	// 禁用拷贝，防止指针浅拷贝导致的 Double Free
	SereinMacroNode(const SereinMacroNode&) = delete;
	SereinMacroNode& operator=(const SereinMacroNode&) = delete;

	SereinMacroNode(SereinMacroNode&& other) noexcept :
		is_expanded(other.is_expanded),
		active_count(other.active_count),
		max_value(other.max_value),
		max_u(other.max_u),
		max_v(other.max_v),
		fine_grid(std::move(other.fine_grid))
	{
		other.reset_metadata();
	}

	SereinMacroNode& operator=(SereinMacroNode&& other) noexcept
	{
		if (this != &other)
		{
			is_expanded = other.is_expanded;
			active_count = other.active_count;
			max_value = other.max_value;
			max_u = other.max_u;
			fine_grid = std::move(other.fine_grid);
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
	}

	void expand(int N, float default_val = 0.0f)
	{
		if (is_expanded)
		{
			return;
		}
		fine_grid = std::make_unique<SereinMicroNode[]>(N * N);
		is_expanded = true;

		float init_val = (max_value != -std::numeric_limits<float>::infinity()) ? max_value : default_val;
		for (int i = 0; i < N * N; ++i)
		{
			fine_grid[i].value = init_val;
			fine_grid[i].active = false;
		}
		max_value = -std::numeric_limits<float>::infinity();
		active_count = 0;
		max_u = -1;
		max_v = -1;
	}

	void recompute_max_value(int N)
	{
		if (!is_expanded || !fine_grid)
		{
			return;
		}
		float current_max = -std::numeric_limits<float>::infinity();
		int best_u = -1, best_v = -1;

		for (int u = 0; u < N; ++u)
		{
			for (int v = 0; v < N; ++v)
			{
				int idx = u * N + v;
				if (fine_grid[idx].active && fine_grid[idx].value > current_max)
				{
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
		if (!is_expanded)
		{
			expand(N);
		}

		int index = u * N + v;
		float old_val = fine_grid[index].value;
		bool old_active = fine_grid[index].active;
		if (std::fabs(val) <= zero_epsilon)
		{
			val = 0.0f;
		}

		fine_grid[index].value = val;
		fine_grid[index].active = val != 0.0f;
		if (!old_active && fine_grid[index].active)
		{
			++active_count;
		}
		else if (old_active && !fine_grid[index].active)
		{
			--active_count;
		}

		if (!fine_grid[index].active)
		{
			if (old_active && old_val == max_value)
			{
				recompute_max_value(N);
			}
			return;
		}

		// O(1) 维护最大值及其坐标
		if (val > max_value)
		{
			max_value = val;
			max_u = u; // 记录坐标
			max_v = v;
		}
		else if (old_active && old_val == max_value && val < old_val)
		{
			recompute_max_value(N); // 原最大值被覆盖变小，重新搜寻
		}
	}

	void collapse()
	{
		fine_grid.reset();
		is_expanded = false;
		active_count = 0;
		max_value = -std::numeric_limits<float>::infinity();
		max_u = -1;
		max_v = -1;
	}

	bool empty() const { return !is_expanded || !fine_grid || active_count == 0; }
};

} // namespace detail

// ==================== 分层空间网格主类 ====================

class SereinGrid
{
private:
	int N; // 展开精度 (1个宏观网格展开为 N x N)
	float spacing;
	float zero_epsilon;
	std::unordered_map<uint64_t, detail::SereinMacroNode> space;

	inline uint64_t get_key(int U, int V) const
	{
		return (static_cast<uint64_t>(static_cast<uint32_t>(U)) << 32) | static_cast<uint32_t>(V);
	}

	inline int floor_div(int a, int b) const { return (a >= 0) ? (a / b) : ((a - b + 1) / b); }

	inline int floor_mod(int a, int b) const
	{
		int r = a % b;
		return r >= 0 ? r : r + b;
	}

	inline bool is_zero(float value) const { return std::fabs(value) <= zero_epsilon; }

public:
	explicit SereinGrid(int expansion_scale = 10, float zero_epsilon = 1e-6f, float physical_spacing = 1.0f) :
		N(expansion_scale),
		spacing(physical_spacing),
		zero_epsilon(zero_epsilon)
	{
		if (N <= 0 || spacing <= 0.0f || zero_epsilon < 0.0f)
		{
			throw std::invalid_argument("Invalid grid scale, physical spacing, or zero epsilon");
		}
	}

	int get_scale() const { return N; }

	float get_spacing() const { return spacing; }

	float get_zero_epsilon() const { return zero_epsilon; }

	// 全局逻辑坐标转换
	void global_to_local(int x, int y, int& U, int& V, int& u, int& v) const
	{
		U = floor_div(x, N);
		V = floor_div(y, N);
		u = floor_mod(x, N);
		v = floor_mod(y, N);
	}

	// 覆盖赋值（支持懒展开）
	void set_value(int x, int y, float value)
	{
		int U, V, u, v;
		global_to_local(x, y, U, V, u, v);
		uint64_t key = get_key(U, V);
		if (is_zero(value))
		{
			auto it = space.find(key);
			if (it == space.end())
			{
				return;
			}
			it->second.set_micro_value(u, v, N, value, zero_epsilon);
			if (it->second.empty())
			{
				space.erase(it);
			}
			return;
		}

		space[key].set_micro_value(u, v, N, value, zero_epsilon);
	}

	// 数值累加
	void add_value(int x, int y, float delta)
	{
		int U, V, u, v;
		global_to_local(x, y, U, V, u, v);

		uint64_t key = get_key(U, V);
		detail::SereinMacroNode& macro = space[key];
		if (!macro.is_expanded)
		{
			macro.expand(N);
		}

		int index = u * N + v;
		float new_val = macro.fine_grid[index].value + delta;
		macro.set_micro_value(u, v, N, new_val, zero_epsilon);
		if (macro.empty())
		{
			space.erase(key);
		}
	}

	// 点平移加法
	void move_point(int src_x, int src_y, int dx, int dy)
	{
		float val = get_value(src_x, src_y);
		if (is_zero(val))
		{
			return;
		}
		set_value(src_x, src_y, 0.0f);
		add_value(src_x + dx, src_y + dy, val);
	}

	// 读取数值
	float get_value(int x, int y) const
	{
		int U, V, u, v;
		global_to_local(x, y, U, V, u, v);

		auto it = space.find(get_key(U, V));
		if (it == space.end() || !it->second.is_expanded)
		{
			return 0.0f;
		}
		const detail::SereinMicroNode& node = it->second.fine_grid[u * N + v];
		return node.active ? node.value : 0.0f;
	}

	// 带 60 度斜坐标判定与 Max-Pooling 剪枝的 NMS
	// 极速版 NMS：利用 max_u/max_v 优先击杀
	bool is_local_maximum(int x, int y, float R_phys) const
	{
		float current_val = get_value(x, y);
		if (current_val <= 0.0f)
		{
			return false;
		}

		int center_U, center_V, center_u, center_v;
		global_to_local(x, y, center_U, center_V, center_u, center_v);

		// 每个内部索引步长对应 spacing / N 的物理距离。
		// 因此物理半径 R_phys 对应的索引域半径为 R_phys * N / spacing。
		double radius_index = static_cast<double>(R_phys) * N / spacing;
		int macro_radius = static_cast<int>(std::ceil(radius_index / N)) + 1;
		double R_sq = radius_index * radius_index;

		for (int dU = -macro_radius; dU <= macro_radius; ++dU)
		{
			for (int dV = -macro_radius; dV <= macro_radius; ++dV)
			{
				int target_U = center_U + dU;
				int target_V = center_V + dV;

				auto it = space.find(get_key(target_U, target_V));
				if (it == space.end() || !it->second.is_expanded)
				{
					continue;
				}

				const detail::SereinMacroNode& target_macro = it->second;

				// 1. 【宏观剪枝】块内最大值都赶不上当前点，跳过整块
				if (target_macro.max_value < current_val)
				{
					continue;
				}

				// 2. 【优先压制判定】已知 target_macro.max_value > current_val，
				// 优先检查该块的绝对峰值点是否在 60 度物理邻域内
				if (target_macro.max_u >= 0 && target_macro.max_v >= 0)
				{
					int peak_x = target_U * N + target_macro.max_u;
					int peak_y = target_V * N + target_macro.max_v;

					if (peak_x != x || peak_y != y)
					{ // 排除自身
						double dx = static_cast<double>(peak_x - x);
						double dy = static_cast<double>(peak_y - y);
						double peak_dist_sq = dx * dx + dx * dy + dy * dy; // 60度斜坐标索引距离平方

						// 峰值点在半径内，瞬时判定“被抑制”，立刻返回！
						if (target_macro.max_value > current_val && peak_dist_sq <= R_sq)
						{
							return false;
						}
					}
				}

				// 3. 【微观精筛补漏】仅当峰值点在半径外时，才遍历块内剩余点
				for (int u = 0; u < N; ++u)
				{
					for (int v = 0; v < N; ++v)
					{
						// 跳过已经检查过的峰值点
						if (u == target_macro.max_u && v == target_macro.max_v)
						{
							continue;
						}

						int nx = target_U * N + u;
						int ny = target_V * N + v;
						if (nx == x && ny == y)
						{
							continue;
						}

						const detail::SereinMicroNode& neighbor = target_macro.fine_grid[u * N + v];
						if (!neighbor.active)
						{
							continue;
						}

						float neighbor_val = neighbor.value;
						if (neighbor_val > current_val)
						{
							double dx = static_cast<double>(nx - x);
							double dy = static_cast<double>(ny - y);
							if (dx * dx + dx * dy + dy * dy <= R_sq)
							{
								return false; // 被块内其他更小但离得更近的更大点抑制
							}
						}
					}
				}
			}
		}
		return true; // 未被任何较大点抑制，为局部极大值
	}

	// 返回整个网格中半径 R_phys 内的局部极大值坐标。
	std::vector<std::pair<int, int>> find_local_maxima(float R_phys) const
	{
		std::vector<std::pair<int, int>> maxima;
		for (const auto& entry : space)
		{
			const detail::SereinMacroNode& macro = entry.second;
			if (!macro.is_expanded || !macro.fine_grid)
			{
				continue;
			}

			int32_t U = static_cast<int32_t>(entry.first >> 32);
			int32_t V = static_cast<int32_t>(entry.first & 0xFFFFFFFFu);
			for (int u = 0; u < N; ++u)
			{
				for (int v = 0; v < N; ++v)
				{
					const detail::SereinMicroNode& node = macro.fine_grid[u * N + v];
					if (node.active && node.value > 0.0f)
					{
						int x = U * N + u;
						int y = V * N + v;
						if (is_local_maximum(x, y, R_phys))
						{
							maxima.emplace_back(x, y);
						}
					}
				}
			}
		}
		return maxima;
	}

	// 两个网格层级合并
	void merge_from(const SereinGrid& other, int offset_x = 0, int offset_y = 0)
	{
		if (N != other.N || spacing != other.spacing)
		{
			throw std::invalid_argument("SereinGrid scales and physical spacing must match");
		}

		for (const auto& entry : other.space)
		{
			const auto& key = entry.first;
			const auto& other_macro = entry.second;
			if (!other_macro.is_expanded)
			{
				continue;
			}

			// 解出 other 的宏观坐标
			int32_t U = static_cast<int32_t>(key >> 32);
			int32_t V = static_cast<int32_t>(key & 0xFFFFFFFF);

			for (int u = 0; u < N; ++u)
			{
				for (int v = 0; v < N; ++v)
				{
					const detail::SereinMicroNode& node = other_macro.fine_grid[u * N + v];
					if (node.active)
					{
						// 1. 计算源网格 (other) 的全局逻辑坐标
						int src_x = U * N + u;
						int src_y = V * N + v;

						// 2. 加上偏移量，算出目标网格 (this) 的全局逻辑坐标
						int dst_x = src_x + offset_x;
						int dst_y = src_y + offset_y;

						// 3. 叠加到当前网格（add_value 内部会自动处理新位置的宏观映射与按需内存分配）
						this->add_value(dst_x, dst_y, node.value);
					}
				}
			}
		}
	}

	// 清空整个网格空间
	void clear() { space.clear(); }
};

} // namespace sereingrid

#endif // SEREIN_GRID_HPP
