# HierarchicalGrid
HierarchicalGrid 是用于稀疏二维空间数据的高性能分层网格组件，专为空间特征提取、动态累加以及基于非极大值抑制（NMS）的局部极值筛选设计。通过两级网格结构（MacroNode / MicroNode），结合 std::unordered_map 的稀疏存储与连续内存块的缓存友好性，实现了低内存占用与高效查询。
