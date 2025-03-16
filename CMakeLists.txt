cmake_minimum_required(VERSION 3.22)
project(MyFantasyTools)

set(CMAKE_CXX_STANDARD 17)

find_package(Python 3.10 EXACT COMPONENTS Interpreter Development REQUIRED)

set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/external)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

add_subdirectory(source/cpp)
add_subdirectory(source/extblender)
