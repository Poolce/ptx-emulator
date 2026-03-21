add_library(execution_module STATIC)

target_sources(execution_module
  PRIVATE
    execution_module.cpp
    execution_module_iterator.cpp
)

target_include_directories(execution_module
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(execution_module PUBLIC encoder)
