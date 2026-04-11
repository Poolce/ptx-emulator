set(RUNTIME_LIB_NAME "emuruntime")

add_library(${RUNTIME_LIB_NAME} SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_def.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rt_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rt_stream.cpp
)

target_include_directories(${RUNTIME_LIB_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${RUNTIME_LIB_NAME} PUBLIC
    context
    encoder
    utils
)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
    target_link_libraries(${RUNTIME_LIB_NAME} PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(${RUNTIME_LIB_NAME} PRIVATE EMULATOR_OPENMP_ENABLED)
endif()
