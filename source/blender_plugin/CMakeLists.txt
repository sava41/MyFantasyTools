
INSTALL(DIRECTORY ./ COMPONENT mft DESTINATION mft_blender_addon)

INSTALL(DIRECTORY ${GENERATED_DIR}/mft/data COMPONENT mft DESTINATION mft_blender_addon/export)

set(BLENDER_WHEELS_DIR ${CMAKE_BINARY_DIR}/wheels)

add_custom_target(download_blender_wheels
    COMMAND ${Python_EXECUTABLE} -m pip download flatbuffers==23.5.26 --dest ${BLENDER_WHEELS_DIR} --no-deps
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(mftools download_blender_wheels)

install(DIRECTORY ${BLENDER_WHEELS_DIR} COMPONENT mft DESTINATION mft_blender_addon)

