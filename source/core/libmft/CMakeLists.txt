add_library(mflib STATIC mf_level.cpp mf_level.h mf_math.h)
add_dependencies(mflib mfutilities flatc generate_flatbuffers)
target_include_directories(mflib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${GENERATED_DIR})
target_link_libraries(mflib PUBLIC mfutilities flatbuffers)

add_executable(mftest main.cpp )
add_dependencies(mftest mflib)
target_link_libraries(mftest PRIVATE mflib)

