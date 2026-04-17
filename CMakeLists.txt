cmake_minimum_required(VERSION 3.20)

project(cpp_template)

option(BUILD_CUDA_SAMPLES OFF "Build CUDA samples")

include(cmake/configure.cmake)

include_directories(include)

enable_testing()

add_subdirectory(3rdparty)
add_subdirectory(app)
add_subdirectory(test)
