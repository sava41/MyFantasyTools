cmake_minimum_required(VERSION 3.22)
project(MyFantasyTools)

set(CMAKE_CXX_STANDARD 17)

find_package(Python 3.11 EXACT COMPONENTS Interpreter Development REQUIRED)

set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/external)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

if(NOT CMAKE_INSTALL_PREFIX)
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
endif()

set(BLENDER_WHEELS_DIR ${CMAKE_BINARY_DIR}/wheels)

add_subdirectory(source/cpp)
add_subdirectory(source/extblender)

add_custom_target(install_python_requirements
    COMMAND ${Python_EXECUTABLE} -m pip download -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt --dest ${BLENDER_WHEELS_DIR} --only-binary=:all: --python-version=3.11 --platform=macosx_11_0_arm64
    COMMAND ${Python_EXECUTABLE} -m pip download -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt --dest ${BLENDER_WHEELS_DIR} --only-binary=:all: --python-version=3.11 --platform=win_amd64
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(mftools install_python_requirements)

install( DIRECTORY ${BLENDER_WHEELS_DIR} COMPONENT mft DESTINATION mft_blender_addon)