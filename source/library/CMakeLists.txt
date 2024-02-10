add_executable(mflib main.cpp level_manager.cpp level_manager.h)
add_dependencies(mflib mfutilities flatc generate_flatbuffers)
target_include_directories(mflib PRIVATE ${GENERATED_DIR})
target_link_libraries(mflib PRIVATE mfutilities flatbuffers)