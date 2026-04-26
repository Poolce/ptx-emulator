set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(encoder STATIC)

target_sources(encoder
  PRIVATE
    function.cpp
    module.cpp
)

target_include_directories(encoder
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/instruction_executors
    "${CMAKE_CURRENT_BINARY_DIR}/autogen"
)

target_link_libraries(encoder PUBLIC utils)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(ENCODER_CODEGEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/codegen")
set(ENCODER_AUTOGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/autogen")

file(GLOB ENCODER_CODEGEN_TEMPLATES CONFIGURE_DEPENDS
  "${ENCODER_CODEGEN_DIR}/templates/*.j2"
)

set(ENCODER_AUTOGEN_SOURCES
  "${ENCODER_AUTOGEN_DIR}/instructions.cpp"
  "${ENCODER_AUTOGEN_DIR}/ptx_types.cpp"
)

set(ENCODER_AUTOGEN_HEADERS
  "${ENCODER_AUTOGEN_DIR}/instructions.h"
  "${ENCODER_AUTOGEN_DIR}/ptx_types.h"
)

add_custom_command(
  OUTPUT
    ${ENCODER_AUTOGEN_SOURCES}
    ${ENCODER_AUTOGEN_HEADERS}
  COMMAND ${CMAKE_COMMAND} -E make_directory "${ENCODER_AUTOGEN_DIR}"
  COMMAND Python3::Interpreter
          "${ENCODER_CODEGEN_DIR}/autogen.py"
          --out "${ENCODER_AUTOGEN_DIR}"
  WORKING_DIRECTORY "${ENCODER_CODEGEN_DIR}"
  DEPENDS
    "${ENCODER_CODEGEN_DIR}/autogen.py"
    "${ENCODER_CODEGEN_DIR}/instructions.json"
    "${ENCODER_CODEGEN_DIR}/profiling_metrics.json"
    ${ENCODER_CODEGEN_TEMPLATES}
  VERBATIM
)

set_source_files_properties(${ENCODER_AUTOGEN_SOURCES} ${ENCODER_AUTOGEN_HEADERS}
  PROPERTIES GENERATED TRUE
)

add_custom_target(encoder_codegen DEPENDS ${ENCODER_AUTOGEN_SOURCES} ${ENCODER_AUTOGEN_HEADERS})
add_dependencies(encoder encoder_codegen)

target_sources(encoder PRIVATE ${ENCODER_AUTOGEN_SOURCES})

target_link_libraries(encoder PUBLIC
  context
  utils
)
