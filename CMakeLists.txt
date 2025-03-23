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

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

set(BLENDER_PACKAGES_DIR ${CMAKE_BINARY_DIR}/blender_packages)
file(MAKE_DIRECTORY ${BLENDER_PACKAGES_DIR})

add_subdirectory(source/cpp)
add_subdirectory(source/extblender)

add_custom_target(install_python_requirements
    COMMAND ${Python_EXECUTABLE} -m pip install -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt --target ${BLENDER_PACKAGES_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(mftools install_python_requirements)

install( DIRECTORY ${BLENDER_PACKAGES_DIR} COMPONENT mft DESTINATION blender_addon/external)