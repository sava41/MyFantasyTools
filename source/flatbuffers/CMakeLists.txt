# Use flatc to generate files

file(GLOB_RECURSE FBS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.fbs")

add_custom_target(generate_flatbuffers ALL
  COMMAND flatc --cpp --python -o ${GENERATED_DIR} ${FBS_FILES}
  COMMENT "Generating FlatBuffers headers from ${FBS_FILES}"
)
add_dependencies(generate_flatbuffers flatc)