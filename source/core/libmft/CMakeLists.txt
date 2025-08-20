add_library(mflib STATIC level_manager.cpp level_manager.h view_data.cpp view_data.h mft_math.h)
add_dependencies(mflib mfutilities flatc generate_flatbuffers)
target_include_directories(mflib PRIVATE ${GENERATED_DIR})
target_include_directories(mflib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mflib PRIVATE mfutilities flatbuffers)

add_executable(mftest main.cpp )
add_dependencies(mftest mflib)
target_link_libraries(mftest PRIVATE mflib)

