file(GLOB sources "*.cpp" "*.h")

add_library(mfutilities STATIC ${sources})
target_include_directories(mfutilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})