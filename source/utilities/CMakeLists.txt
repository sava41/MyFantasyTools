file(GLOB sources "*.cpp" "*.h")

add_library(mfutilities STATIC ${sources})
target_include_directories(mfutilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(mfutilities jxl jxl_threads generate_flatbuffers)
target_link_libraries(mfutilities PRIVATE jxl jxl_threads)