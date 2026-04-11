add_executable(cuemu main.cpp)

target_sources(cuemu
  PRIVATE
    main.cpp
)

target_include_directories(cuemu
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cuemu PUBLIC
    encoder
    emuruntime
)
