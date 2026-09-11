cmake_minimum_required(VERSION 3.14)
project(HierarchicalGrid VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only interface target
add_library(HierarchicalGrid INTERFACE)
target_include_directories(HierarchicalGrid INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 构建选项：默认开启测试与示例
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

if(BUILD_EXAMPLES)
    add_executable(basic_usage examples/basic_usage.cpp)
    target_link_libraries(basic_usage PRIVATE HierarchicalGrid)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_executable(grid_tests tests/test_grid.cpp)
    target_link_libraries(grid_tests PRIVATE HierarchicalGrid)
    add_test(NAME grid_tests COMMAND grid_tests)
endif()