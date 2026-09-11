#include "../include/SereinGrid.hpp"
#include <iostream>
#include <vector>

int main()
{
    std::cout << "=== SereinGrid 基础使用示例 ===\n\n";

    // 1. 初始化分层网格
    // 参数含义：宏观网格展开尺寸 N=10, 零值阈值=1e-6, 物理间距=1.0f
    int scale = 10;
    float zero_eps = 1e-6f;
    float spacing = 1.0f;
    sereingrid::SereinGrid grid(scale, zero_eps, spacing);

    std::cout << "[1] 网格配置信息:\n";
    std::cout << " - 展开尺度 (N): " << grid.get_scale() << "x" << grid.get_scale() << "\n";
    std::cout << " - 物理点距 (Spacing): " << grid.get_spacing() << "\n";
    std::cout << " - 零值判断阈值 (Zero Epsilon): " << grid.get_zero_epsilon() << "\n\n";

    // 2. 基础数值写入与读取（支持正负坐标与跨宏观块存储）
    std::cout << "[2] 写入并读取测试数据...\n";
    grid.set_value(5, 5, 10.5f);
    grid.set_value(-12, 8, 20.0f);   // 位于负 X 轴区域
    grid.set_value(-15, -25, 15.0f); // 位于第三象限

    std::cout << " - 点 (5, 5) 的值: " << grid.get_value(5, 5) << " (期望: 10.5)\n";
    std::cout << " - 点 (-12, 8) 的值: " << grid.get_value(-12, 8) << " (期望: 20)\n";
    std::cout << " - 未设置点 (0, 0) 的值: " << grid.get_value(0, 0) << " (期望: 0)\n\n";

    // 3. 点平移与累加操作
    std::cout << "[3] 测试点平移 (move_point) 与数值累加 (add_value)...\n";
    grid.add_value(5, 5, 4.5f); // 10.5 + 4.5 = 15.0
    std::cout << " - 累加后点 (5, 5) 的值: " << grid.get_value(5, 5) << " (期望: 15)\n";

    // 将 (5, 5) 移动 dx=10, dy=0 -> 新位置 (15, 5)
    grid.move_point(5, 5, 10, 0);
    std::cout << " - 移动后原点 (5, 5) 的值: " << grid.get_value(5, 5) << " (期望: 0)\n";
    std::cout << " - 移动后新点 (15, 5) 的值: " << grid.get_value(15, 5) << " (期望: 15)\n\n";

    // 4. 局部极大值 (NMS) 查找示例
    std::cout << "[4] 测试局部极大值查找 (find_local_maxima)...\n";
    grid.clear(); // 清空网格重新构造测试场景

    // 设置几处热点（包含邻域抑制场景）
    // 场景 A: 两个较近的点，(0,0) 强度为 8.0，(1,1) 强度为 5.0。
    // 在 60° 斜坐标物理半径 R=3.0 内，(1,1) 应被 (0,0) 抑制。
    grid.set_value(0, 0, 8.0f);
    grid.set_value(1, 1, 5.0f);

    // 场景 B: 较远的独立峰值点
    grid.set_value(20, 20, 12.0f);
    grid.set_value(-30, -30, 9.5f);

    float R_phys = 3.0f; // 物理抑制半径
    std::vector<std::pair<int, int>> maxima = grid.find_local_maxima(R_phys);

    std::cout << " - 在半径 R_phys=" << R_phys << " 下找到的局部极大值点坐标:\n";
    for (const auto& pt : maxima) {
        std::cout << "   -> 坐标: (" << pt.first << ", " << pt.second
                  << ") | 强度: " << grid.get_value(pt.first, pt.second) << "\n";
    }
    std::cout << "\n";

    // 5. 多图层/网格合并 (merge_from)
    std::cout << "[5] 测试网格合并与坐标偏移 (merge_from)...\n";
    sereingrid::SereinGrid sub_grid(scale, zero_eps, spacing);
    sub_grid.set_value(0, 0, 3.0f);
    sub_grid.set_value(2, 2, 7.0f);

    // 将 sub_grid 合并至 main grid，施加物理坐标偏移 offset_x=10, offset_y=10
    grid.merge_from(sub_grid, 10, 10);

    std::cout << " - 合并后目标网格 (10, 10) 的值: " << grid.get_value(10, 10) << " (期望: 3)\n";
    std::cout << " - 合并后目标网格 (12, 12) 的值: " << grid.get_value(12, 12) << " (期望: 7)\n";

    std::cout << "\n=== 示例运行完毕 ===\n";
    return 0;
}
