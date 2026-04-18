message(STATUS "Building CUDA samples")

find_program(NVCC_EXECUTABLE nvcc)
if(NOT NVCC_EXECUTABLE)
  message(FATAL_ERROR "nvcc not found. Install CUDA toolkit or make nvcc available on PATH.")
endif()

set(SAMPLE_SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB SAMPLE_SOURCES "${SAMPLE_SOURCES_DIR}/*.cu")
if(NOT SAMPLE_SOURCES)
  message(FATAL_ERROR "No CUDA source files found in ${SAMPLE_SOURCES_DIR}")
endif()

set(SAMPLE_EXECUTABLES "")
foreach(SAMPLE_SOURCE IN LISTS SAMPLE_SOURCES)
  get_filename_component(SAMPLE_NAME ${SAMPLE_SOURCE} NAME_WE)
  set(SAMPLE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cuda_samples")
  set(SAMPLE_EXECUTABLE "${SAMPLE_OUTPUT_DIR}/${SAMPLE_NAME}")

  add_custom_command(
    OUTPUT ${SAMPLE_EXECUTABLE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SAMPLE_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Building sample: ${SAMPLE_NAME}"
    COMMAND ${NVCC_EXECUTABLE} ${SAMPLE_SOURCE} -o ${SAMPLE_EXECUTABLE} --cudart=shared
    DEPENDS ${SAMPLE_SOURCE}
    WORKING_DIRECTORY ${SAMPLE_SOURCES_DIR}
    VERBATIM
  )

  list(APPEND SAMPLE_EXECUTABLES ${SAMPLE_EXECUTABLE})
endforeach()

add_custom_target(cuda_samples ALL DEPENDS ${SAMPLE_EXECUTABLES})
