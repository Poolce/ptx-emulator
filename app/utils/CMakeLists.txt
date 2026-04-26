set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(utils STATIC gpu_config.cpp)

target_include_directories(utils
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
