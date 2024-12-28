cmake_minimum_required(VERSION 3.31)

project(
    load_font
    VERSION 0.1
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(lib)

add_executable(load_font main.c plain_chunk_list.c)

target_link_libraries(load_font PUBLIC riff logging)

# include(cmake/CPM.cmake)

# find_package()

# cpmaddpackage()

# target_link_libraries()

target_compile_options(
    load_font PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wall -Weverything> $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
                               $<$<CXX_COMPILER_ID:MSVC>:/W4>)
