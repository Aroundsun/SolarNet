cmake_minimum_required(VERSION 3.20)
project(
  SolarNet
  VERSION 0.1.0
  DESCRIPTION "现代 C++20 Linux 网络框架"
  LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# 构建选项
# -----------------------------------------------------------------------------
option(SOLAR_NET_BUILD_TESTS "构建单元测试" ON)
option(SOLAR_NET_BUILD_BENCHMARKS "构建基准测试" ON)
option(SOLAR_NET_BUILD_EXAMPLES "构建示例程序" ON)
option(SOLAR_NET_BUILD_SHARED "构建动态库（默认静态库）" OFF)
option(SOLAR_NET_USE_CLANG_TIDY "构建时启用 clang-tidy 检查" ON)

# -----------------------------------------------------------------------------
# C++ 标准
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# 警告与编译选项
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
  )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  add_compile_options(/W4 /WX)
endif()

# -----------------------------------------------------------------------------
# clang-tidy
# -----------------------------------------------------------------------------
if(SOLAR_NET_USE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY
      "${CLANG_TIDY_EXE}"
      --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
    )
  else()
    message(STATUS "未找到 clang-tidy，已跳过")
  endif()
endif()

# -----------------------------------------------------------------------------
# 源码与头文件目录
# -----------------------------------------------------------------------------
# 设计选择：头文件与实现文件同置于 solar_net/ 下，
# 保持目录扁平，无需在 include/ 与 src/ 之间跳转即可对应 .h 与 .cpp。
set(SOLAR_NET_PUBLIC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SOLAR_NET_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/solar_net)

file(GLOB_RECURSE SOLAR_NET_HEADERS
  CONFIGURE_DEPENDS
  ${SOLAR_NET_SOURCE_DIR}/*.h
)
file(GLOB_RECURSE SOLAR_NET_SOURCES
  CONFIGURE_DEPENDS
  ${SOLAR_NET_SOURCE_DIR}/*.cpp
)

# -----------------------------------------------------------------------------
# 库目标
# -----------------------------------------------------------------------------
if(SOLAR_NET_BUILD_SHARED)
  add_library(solar_net SHARED ${SOLAR_NET_SOURCES})
else()
  add_library(solar_net STATIC ${SOLAR_NET_SOURCES})
endif()

target_include_directories(solar_net
  PUBLIC
    $<BUILD_INTERFACE:${SOLAR_NET_PUBLIC_DIR}>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(solar_net PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  EXPORT_NAME SolarNet
)

# Linux 网络相关宏
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(solar_net PUBLIC _GNU_SOURCE)
endif()

# -----------------------------------------------------------------------------
# 测试与基准测试依赖（需系统已安装对应开发包）
# -----------------------------------------------------------------------------
if(SOLAR_NET_BUILD_TESTS)
  find_package(GTest REQUIRED)
  include(GoogleTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SOLAR_NET_BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)
  add_subdirectory(benchmarks)
endif()

if(SOLAR_NET_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# -----------------------------------------------------------------------------
# 安装
# -----------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS solar_net
  EXPORT SolarNetTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY solar_net
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
