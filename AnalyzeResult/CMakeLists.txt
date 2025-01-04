cmake_minimum_required(VERSION 3.31)

project(
   analyzer 
    VERSION 0.1
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(analyzer analyzer.cpp)

# include(cmake/CPM.cmake)

find_package(OpenCV REQUIRED)

target_link_libraries(analyzer PUBLIC ${OpenCV_LIBS} )

target_include_directories(analyzer PUBLIC ${OpenCV_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/progressbar/include)

target_compile_options(
    analyzer PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wall -Weverything> $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra >
                               $<$<CXX_COMPILER_ID:MSVC>:/W4>)
