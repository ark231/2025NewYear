cmake_minimum_required(VERSION 3.31)

project(
    simple_logging
    VERSION 0.1
    LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(logging simple_logging.c simple_logging.h)

target_include_directories(logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
                                            

# include(cmake/CPM.cmake)

# find_package()

# cpmaddpackage()

# target_link_libraries()

target_compile_options(
    logging PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wall -Weverything> $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
                               $<$<CXX_COMPILER_ID:MSVC>:/W4>)
