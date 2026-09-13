# SereinGrid

![Language](https://img.shields.io/badge/language-C%2B%2B14-brightgreen.svg)

`SereinGrid` 是一个用于稀疏二维空间数据的高性能分层网格库，适用于空间特征提取、动态累加以及基于非极大值抑制（NMS）的局部极大值筛选。

它采用两级稀疏网格结构：宏观块级索引和微观单元级存储，结合 `std::unordered_map` 的稀疏布局与连续内存块的缓存友好性，实现了低内存占用、高查询效率，以及对负坐标的原生支持。

## 项目定位

- 目标场景：二维稀疏空间、动态扩张、局部极大值筛选
- 设计特点：
  - 宏观块级懒加载
  - 稀疏存储，避免全局二维数组开销
  - 负坐标兼容
  - 统一的全局坐标 / 宏观坐标 / 局部坐标转换
  - 基于物理距离的 NMS 检测

## 快速集成

本项目是 Header-only 库，仅依赖 C++14 标准库和 STL。

### 方式 1：直接引入头文件

将仓库中的 `include/SereinGrid.hpp` 复制到你的项目头文件目录，然后这样使用：

```cpp
#include "SereinGrid.hpp"

int main()
{
    sereingrid::SereinGrid grid(10, 1e-6f, 1.0f);
    grid.set_value(5, 7, 0.8f);
    std::cout << grid.get_value(5, 7) << '\n';
    return 0;
}
```

### 方式 2：通过 CMake FetchContent 使用

在你的 `CMakeLists.txt` 中添加：

```cmake
include(FetchContent)

FetchContent_Declare(
    SereinGrid
    GIT_REPOSITORY https://github.com/RainYangty/SereinGrid.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(SereinGrid)

target_link_libraries(your_target PRIVATE SereinGrid)
```

## 代码结构

```text
SereinGrid/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── SereinGrid.hpp                     # 公共兼容入口
│   └── sereingrid/
│       ├── SereinGrid.hpp                # 核心类声明
│       ├── SereinGrid.inl                # 实现定义
│       └── detail/
│           ├── Coordinates.hpp           # 坐标转换与键生成
│           └── Nodes.hpp                 # 宏观/微观节点定义
├── tests/
│   └── test_sereingrid.cpp              # 单元测试
├── examples/
│   └── sereingrid_basic_usage.cpp       # 基础示例
└── build/
```

## 模块设计

### 1. 宏观节点与微观节点

- `SereinMicroNode`：表示最小格点单元
  - `value`: 当前格点数值
  - `active`: 是否已激活
- `SereinMacroNode`：表示宏观块，内部持有一个连续的微观网格数组
  - 对应块大小为 $N \times N$
  - 支持按需扩展与懒分配
  - 维护当前块的最大值和最大值位置，以便后续 NMS 剪枝

### 2. 稀疏空间索引

网格使用 64 位整数键来管理宏观块：

```cpp
key = (U << 32) | V;
```

其中 `U` 和 `V` 为宏观块坐标，`SereinGrid` 通过 `std::unordered_map<uint64_t, SereinMacroNode>` 实现稀疏存储。

### 3. 负坐标支持

库内部使用 `floor_div` / `floor_mod` 处理负数除法和取模，保证在负坐标轴上仍然能稳定地映射到相同的块与局部坐标。

## API 说明

### 初始化与坐标转换

| 接口 | 签名 | 说明 |
| --- | --- | --- |
| `SereinGrid` | `SereinGrid(int expansion_scale = 10, float zero_epsilon = 1e-6f, float physical_spacing = 1.0f)` | 构造函数，设置块大小、零值容差和物理间距 [`(x, y)`与`(x + 1, y)`间距] |
| `get_scale` | `int get_scale() const` | 返回宏观块大小 `N` |
| `get_spacing` | `float get_spacing() const` | 返回物理间距 |
| `get_zero_epsilon` | `float get_zero_epsilon() const` | 返回零值判定容差 |
| `global_to_local` | `void global_to_local(int x, int y, int& U, int& V, int& u, int& v) const` | 将全局坐标转换为宏观/局部坐标 |
| `macro_to_global` | `void macro_to_global(int U, int V, int& x, int& y) const` | 将宏观块坐标转换回宏观块原点的全局坐标 |
| `local_to_global` | `void local_to_global(int U, int V, int u, int v, int& x, int& y) const` | 将宏观坐标和局部坐标组合成全局坐标 |

### 数据读写

| 接口 | 签名 | 说明 |
| --- | --- | --- |
| `set_value` | `void set_value(int x, int y, float value)` | 写入坐标 `(x, y)` 的值；当值接近 0 时自动清理节点 |
| `add_value` | `void add_value(int x, int y, float delta)` | 在某点累加值 |
| `move_point` | `void move_point(int src_x, int src_y, int dx, int dy)` | 将值从一个位置移动到另一个位置 |
| `get_value` | `float get_value(int x, int y) const` | 读取位置值，若不存在则返回 `0.0f` |
| `clear` | `void clear()` | 清空所有数据 |

### NMS 与合并

| 接口 | 签名 | 说明 |
| --- | --- | --- |
| `is_local_maximum` | `bool is_local_maximum(int x, int y, float R_phys) const` | 判断某点是否在半径 `R_phys` 内为局部极大值 |
| `find_local_maxima` | `std::vector<std::pair<int, int>> find_local_maxima(float R_phys) const` | 返回所有局部极大值坐标 |
| `merge_from` | `void merge_from(const SereinGrid& other, int offset_x = 0, int offset_y = 0)` | 将另一张网格数据融合到当前网格，可带坐标偏移 |

## 核心算法

### 1. 坐标映射

全局坐标 $(x, y)$ 转换为宏观块坐标 $(U, V)$ 及局部坐标 $(u, v)$ 的规则如下：

$$
U = \left\lfloor \frac{x}{N} \right\rfloor, \quad V = \left\lfloor \frac{y}{N} \right\rfloor
$$

$$
u = x \bmod N, \quad v = y \bmod N
$$

其中 `N` 是宏观块大小。由于使用了负坐标兼容的整除/取模逻辑，因此该映射在四个象限中都保持一致。

### 2. NMS 剪枝

`is_local_maximum` 和 `find_local_maxima` 使用了三层剪枝思路：

1. 宏观块级剪枝：如果邻近块的最大值明显小于当前点，则不必深入该块。
2. 极值优先判定：如果邻近块已记录的最大值能确定当前点被抑制，则直接短路。
3. 微观精筛：只有在必要时才逐个检查块内具体位置。

### 3. 空间距离

当前实现使用 60° 斜坐标系的距离度量：

$$
\text{Dist}^2 = dx^2 + dx \cdot dy + dy^2
$$

该距离形式在六边形或菱形采样结构中比传统欧式距离更接近真实邻域关系。

## 使用示例

```cpp
#include <iostream>
#include "SereinGrid.hpp"

int main()
{
    sereingrid::SereinGrid grid(10, 1e-6f, 1.0f);

    grid.set_value(0, 0, 10.0f);
    grid.set_value(1, 1, 6.0f);
    grid.set_value(-5, -5, 7.5f);

    int U, V, u, v;
    int x, y;

    grid.global_to_local(-5, -5, U, V, u, v);
    grid.local_to_global(U, V, u, v, x, y);

    std::cout << "global_to_local: (U, V, u, v) = (" << U << ", " << V << ", " << u << ", " << v << ")\n";
    std::cout << "round-trip: (x, y) = (" << x << ", " << y << ")\n";

    auto maxima = grid.find_local_maxima(3.0f);
    std::cout << "local maxima count: " << maxima.size() << '\n';

    return 0;
}
```

## 备注

- 这是一个头文件型库，编译无需额外链接。
- 单元测试位于 [tests/test_sereingrid.cpp](tests/test_sereingrid.cpp)。
- 示例程序位于 [examples/sereingrid_basic_usage.cpp](examples/sereingrid_basic_usage.cpp)。
- 当前仓库已通过 CMake + CTest 构建和测试校验。

## 许可证

当前仓库未附带显式许可证文件，使用前请确认目标项目的许可要求。