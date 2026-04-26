set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(context STATIC)

target_sources(context
  PRIVATE
    warp_context.cpp
    block_context.cpp
    global_context.cpp
    profiler.cpp
)

target_include_directories(context
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(context PUBLIC encoder utils)
