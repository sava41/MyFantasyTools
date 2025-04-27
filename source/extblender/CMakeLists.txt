
INSTALL(DIRECTORY ./ COMPONENT mft DESTINATION ${PROJECT_SOURCE_DIR}/mft_blender_addon)

INSTALL(DIRECTORY ${GENERATED_DIR}/mft/data COMPONENT mft DESTINATION ${PROJECT_SOURCE_DIR}/mft_blender_addon/export)

set(BLENDER_WHEELS_DIR ${CMAKE_BINARY_DIR}/wheels)

add_custom_target(install_python_requirements
    COMMAND ${Python_EXECUTABLE} -m pip download -r ${PROJECT_SOURCE_DIR}/requirements.txt --dest ${BLENDER_WHEELS_DIR} --only-binary=:all: --python-version=3.11 --platform=macosx_11_0_arm64
    COMMAND ${Python_EXECUTABLE} -m pip download -r ${PROJECT_SOURCE_DIR}/requirements.txt --dest ${BLENDER_WHEELS_DIR} --only-binary=:all: --python-version=3.11 --platform=win_amd64
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(mftools install_python_requirements)

install( DIRECTORY ${BLENDER_WHEELS_DIR} COMPONENT mft DESTINATION ${PROJECT_SOURCE_DIR}/mft_blender_addon)