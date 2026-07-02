cmake_minimum_required(VERSION 3.20)
project(
  SolarNet
  VERSION 0.1.0
  DESCRIPTION "A modern C++20 Linux network framework"
  LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(SOLAR_NET_BUILD_TESTS "Build unit tests" ON)
option(SOLAR_NET_BUILD_BENCHMARKS "Build benchmarks" ON)
option(SOLAR_NET_BUILD_EXAMPLES "Build examples" ON)
option(SOLAR_NET_BUILD_SHARED "Build shared library instead of static" OFF)
option(SOLAR_NET_USE_CLANG_TIDY "Enable clang-tidy checks" ON)

# -----------------------------------------------------------------------------
# C++ standard
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Warnings and compile flags
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
    message(STATUS "clang-tidy not found, skipping")
  endif()
endif()

# -----------------------------------------------------------------------------
# Source and include directories
# -----------------------------------------------------------------------------
# Design choice: headers and sources live together under solar_net/.
# This keeps the project layout flat and makes it obvious which .cpp implements
# which .h without jumping between include/ and src/.
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
# Library target
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

# Linux-specific network flags
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(solar_net PUBLIC _GNU_SOURCE)
endif()

# -----------------------------------------------------------------------------
# Testing and benchmarking dependencies (system packages required)
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
# Install
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
