cmake_minimum_required(VERSION 3.20)

project(cpp_template)

include(cmake/configure.cmake)

include_directories(include)

enable_testing()

add_subdirectory(3rdparty)
add_subdirectory(app)
# add_subdirectory(src)
add_subdirectory(test)
