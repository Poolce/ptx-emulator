set(RUNTIME_LIB_NAME "emuruntime")

add_library(${RUNTIME_LIB_NAME} SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_def.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rt_interface.cpp
)

target_include_directories(${RUNTIME_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${RUNTIME_LIB_NAME} PUBLIC
    execution_module
    context
    encoder
)
