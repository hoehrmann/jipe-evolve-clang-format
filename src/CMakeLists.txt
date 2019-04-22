cmake_minimum_required (VERSION 3.1)

find_package(Clang REQUIRED CONFIG)

message(STATUS "Found Clang library")

set (CMAKE_CXX_STANDARD 11)

add_executable(
  jipe-evolve-clang-format jipe-evolve-clang-format.cxx)

target_link_libraries(
  jipe-evolve-clang-format PRIVATE git2 clangTooling)

target_include_directories(
  jipe-evolve-clang-format PRIVATE ${CLANG_INCLUDE_DIRS} )

target_compile_options(
  jipe-evolve-clang-format PRIVATE -Wall -Wextra -pedantic)
  