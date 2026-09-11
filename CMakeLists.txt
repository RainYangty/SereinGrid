cmake_minimum_required(VERSION 3.14)
project(SereinGrid VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only interface target
add_library(SereinGrid INTERFACE)
target_include_directories(SereinGrid INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 构建选项：默认开启测试与示例
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

if(BUILD_EXAMPLES)
    add_executable(sereingrid_basic_usage examples/sereingrid_basic_usage.cpp)
    target_link_libraries(sereingrid_basic_usage PRIVATE SereinGrid)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_executable(sereingrid_tests tests/test_sereingrid.cpp)
    target_link_libraries(sereingrid_tests PRIVATE SereinGrid)
    add_test(NAME sereingrid_tests COMMAND sereingrid_tests)
endif()