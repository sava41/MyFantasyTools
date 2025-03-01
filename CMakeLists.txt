cmake_minimum_required(VERSION 3.22)
project(MyFantasyTools)

set(CMAKE_CXX_STANDARD 17)

#-------Find Python-------
find_package(Python 3.10 EXACT COMPONENTS Interpreter Development REQUIRED)

#-------Set Up Third Party Dependencies-------
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/third_party)

include(FetchContent)

message("Configuring libjxl")
FetchContent_Declare(
  libjxl
  GIT_REPOSITORY https://github.com/libjxl/libjxl.git
  GIT_TAG v0.9.2
)
set(JPEGXL_STATIC true)
FetchContent_GetProperties(libjxl)
if(NOT libjxl_POPULATED)
  FetchContent_Populate(libjxl)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  add_subdirectory(${libjxl_SOURCE_DIR} ${libjxl_BINARY_DIR})
endif()

message("Configuring flatbuffers")
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v23.5.26
)
FetchContent_GetProperties(flatbuffers)
if(NOT flatbuffers_POPULATED)
  FetchContent_Populate(flatbuffers)
  add_subdirectory(${flatbuffers_SOURCE_DIR} ${flatbuffers_BINARY_DIR})
endif()

message("Configuring pybind")
FetchContent_Declare(
  pybind
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.11.1
)
FetchContent_GetProperties(pybind)
if(NOT pybind_POPULATED)
  FetchContent_Populate(pybind)
  add_subdirectory(${pybind_SOURCE_DIR} ${pybind_BINARY_DIR})
endif()

#-------Compile MFT Tools and Libraries-------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_subdirectory(source/flatbuffers)
add_subdirectory(source/utilities)
add_subdirectory(source/python)
add_subdirectory(source/library)

# configure installation
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})