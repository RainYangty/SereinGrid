# HierarchicalGrid

![Language](https://img.shields.io/badge/language-C%2B%2B14-brightgreen.svg)

`HierarchicalGrid` 是用于稀疏二维空间数据的高性能分层网格组件，专为空间特征提取、动态累加以及基于非极大值抑制（NMS）的局部极值筛选设计。通过两级网格结构（`MacroNode` / `MicroNode`），结合 `std::unordered_map` 的稀疏存储与连续内存块的缓存友好性，实现了低内存占用与高效查询。


## 快速集成

本项目为 Header-only 库，仅依赖 C++14 及以上标准库。

### 方法 1：直接引入头文件

将 `include/HierarchicalGrid.hpp` 复制到项目头文件目录中即可：

```cpp
#include "HierarchicalGrid.hpp"
```

### 方法 2：通过 CMake FetchContent 引入

在项目的 `CMakeLists.txt` 中添加：

```cmake
include(FetchContent)
FetchContent_Declare(
    HierarchicalGrid
    GIT_REPOSITORY https://github.com/your_username/HierarchicalGrid.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(HierarchicalGrid)

target_link_libraries(your_target PRIVATE HierarchicalGrid)
```


## 模块架构设计

网格采用分层展开与动态懒加载机制，解决稀疏空间下内存浪费与遍历开销大的问题。

```text
HierarchicalGrid (unordered_map<uint64_t, MacroNode>)
 ├── Key: (U << 32) | V  (宏观空间块索引)
 └── MacroNode (元数据 + 极值追踪)
      └── fine_grid -> unique_ptr<MicroNode[]>(N * N) (按需分配的连续内存块)

```

### 数据结构定义

* **`MicroNode`**：最小空间数据节点。
* `float value`：节点的特征值或置信度。
* `bool active`：标识节点是否被有效赋值。


* **`MacroNode`**：宏观网格块（包含 $N \times N$ 个微观节点）。
* **懒加载管理**：初始化时不分配微观网格内存，仅在首次写入数据时调用 `expand()` 动态分配连续数组。
* **$O(1)$ 极值追踪**：内部实时维护块内极大值 `max_value` 及其微观坐标 `(max_u, max_v)`，为后续 NMS 提供高效宏观剪枝策略。
* **内存安全**：基于 `std::unique_ptr` 管理连续内存，显式禁用拷贝语义，实现高效且安全的移动语义，彻底避免 `std::unordered_map` 在 Rehash 时的二次释放与深拷贝开销。


* **`HierarchicalGrid`**：全局空间映射网格。
* 基于 64 位整型 Key 映射宏观坐标 $(U, V)$。
* 内置向负无穷取整的整除与取模算法（`floor_div` / `floor_mod`），原生支持四个象限的负坐标访问。




## API 接口规范

### 1. 初始化与配置
<!-- markdownlint-disable -->
| 接口名称 | 参数定义 | 说明 |
| --- | --- | --- |
| `HierarchicalGrid` | `int expansion_scale = 10, float zero_epsilon = 1e-6f, float physical_spacing = 1.0f` | 构造函数。配置宏观块展开尺度 $N$（即块大小 $N \times N$）、零值判决容差 tolerance 及物理采样间距。 |
| `get_scale` | - | 返回宏观块尺度 $N$。 |
| `get_spacing` | - | 返回采样点的物理间距 `spacing`。 |
| `get_zero_epsilon` | - | 返回判决零值的容差阈值 `zero_epsilon`。 |
<!-- markdownlint-restore -->
### 2. 数据读写与更新

| 接口名称 | 参数定义 | 说明 |
| --- | --- | --- |
| `set_value` | `int x, int y, float value` | 覆盖写入坐标 $(x, y)$ 的数值。若 `value` 为 0（或小于容差），自动清除节点；若导致宏观块清空则自动收回映射。 |
| `add_value` | `int x, int y, float delta` | 在坐标 $(x, y)$ 处累加数值，自动触发宏观块的按需展开。 |
| `get_value` | `int x, int y` | 查询坐标 $(x, y)$ 的数值。若处于未展开块或未激活节点，直接返回 `0.0f`。 |
| `move_point` | `int src_x, int src_y, int dx, int dy` | 原子化平移操作。将 `(src_x, src_y)` 的值清零并累加至 `(src_x + dx, src_y + dy)`。 |
| `clear` | - | 释放所有已分配的宏观节点及微观数组内存。 |

### 3. NMS 极值提取与空间合并

| 接口名称 | 参数定义 | 返回值 | 说明 |
| --- | --- | --- | --- |
| `is_local_maximum` | `int x, int y, float R_phys` | `bool` | 判断坐标 $(x, y)$ 是否为物理半径 `R_phys` 范围内的局部极大值（内建三层剪枝算法）。 |
| `find_local_maxima` | `float R_phys` | `std::vector<std::pair<int, int>>` | 遍历网格内所有激活点，提取物理半径 `R_phys` 范围内的局部极大值坐标集合。 |
| `merge_from` | `const HierarchicalGrid& other, int offset_x = 0, int offset_y = 0` | `void` | 将另一个网格的数据图层融合至当前网格，支持施加全局空间坐标偏移。 |



## 核心算法细节

### 1. 两级坐标映射

全局坐标 $(x, y)$ 到宏观块坐标 $(U, V)$ 及微观局部坐标 $(u, v)$ 的映射公式如下：

$$U = \lfloor x / N \rfloor, \quad V = \lfloor y / N \rfloor$$

$$u = x \pmod N, \quad v = y \pmod N$$

代码内部通过 `floor_div` 与 `floor_mod` 保证负坐标轴的连续性与正向对齐。

### 2. 带极值剪枝的极速 NMS 算法

`is_local_maximum` 采用 **Max-Pooling 思想** 进行三层加速判定：

1. **宏观块剪枝**：遍历邻近宏观块时，若目标块的 `target_macro.max_value < current_val`，说明该块内无任何节点能抑制当前点，直接跳过整个 $N \times N$ 块。
2. **优先压制判定**：若目标块的极大值大于当前点，优先获取该块记录的最高点坐标 `(max_u, max_v)`。若该最高点落在半径域内，直接断言当前点被抑制，瞬间退出循环（$O(1)$ 击杀）。
3. **微观精筛**：仅当最高点在半径域外、但块内可能存在较小但距离更近的优势点时，才退化为微观节点遍历。

### 3. 60 度斜坐标系空间度量

距离判决计算采用了 60 度斜角坐标系（Hexagonal / Rhombohedral Metric）：

$$\text{Dist}^2 = dx^2 + dx \cdot dy + dy^2$$

相较于传统直角坐标系，该度量方式在等边三角形/六边形空间采样点阵中具有更优的各向同性。


## 使用示例

```cpp
#include <iostream>
#include "HierarchicalGrid.hpp"

int main()
{
    // 初始化网格：展开尺度 N=10，零容差 1e-5，物理间距 0.5m
    hgrid::HierarchicalGrid grid(10, 1e-5f, 0.5f);

    // 写入模拟置信度数据
    grid.set_value(15, 20, 0.85f);
    grid.set_value(16, 20, 0.92f); // 邻接高值点，应抑制 (15, 20)
    grid.set_value(-5, -5, 0.78f); // 负坐标点

    // 查询指定点
    std::cout << "Value at (16, 20): " << grid.get_value(16, 20) << std::endl;

    // 执行半径 R_phys = 1.0m 的非极大值抑制 (NMS)
    auto maxima = grid.find_local_maxima(1.0f);

    std::cout << "Detected Local Maxima Count: " << maxima.size() << std::endl;
    for (const auto& [x, y] : maxima)
    {
        std::cout << "Max at (" << x << ", " << y << ") = " << grid.get_value(x, y) << std::endl;
    }

    return 0;
}

```