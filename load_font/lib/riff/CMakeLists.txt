cmake_minimum_required(VERSION 3.31)

project(
    lib_riff
    VERSION 0.1
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(riff riff_reader.c riff_reader.h)

target_include_directories(riff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(riff PUBLIC logging)

# include(cmake/CPM.cmake)

# find_package()

# cpmaddpackage()

# target_link_libraries()

target_compile_options(
    riff PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wall -Weverything> $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
                               $<$<CXX_COMPILER_ID:MSVC>:/W4>)

