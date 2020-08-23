cmake_minimum_required(VERSION 3.0.0)
project(zpack)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(programs)
add_subdirectory(lib)