add_library(mflib STATIC mf_view_resources.cpp mf_view_resources.h mf_math.h mf_level.h mf_level.cpp)
add_dependencies(mflib mfutilities flatc generate_flatbuffers)
target_include_directories(mflib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GENERATED_DIR})
target_link_libraries(mflib PUBLIC mfutilities flatbuffers)

add_executable(mftest main.cpp )
add_dependencies(mftest mflib)
target_link_libraries(mftest PRIVATE mflib)

