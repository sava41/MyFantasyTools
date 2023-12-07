cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

project(MyFantasyTools)

#-------Find Python-------
find_package(Python 3.10 EXACT COMPONENTS Interpreter Development REQUIRED)

#-------Set Up Third Party Dependencies-------
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/third_party)

message("Configuring libjxl")
FetchContent_Declare(
  libjxl
  GIT_REPOSITORY https://github.com/libjxl/libjxl.git
  GIT_TAG v0.8.2
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

# Use flatc to generate files
set(FBS_FILES ${CMAKE_SOURCE_DIR}/source/flatbuffers/level.fbs)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated_flatbuffers)
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_target(generate_flatbuffers ALL
  COMMAND flatc --cpp --python -o ${GENERATED_DIR} ${FBS_FILES}
  COMMENT "Generating FlatBuffers headers from ${FBS_FILES}"
)
add_dependencies(generate_flatbuffers flatc)

add_executable(mft_lib ${PROJECT_SOURCE_DIR}/source/main.cpp)
add_dependencies(mft_lib flatc jxl jxl_threads generate_flatbuffers)
target_include_directories(mft_lib PRIVATE ${GENERATED_DIR})
target_link_libraries(mft_lib PRIVATE flatbuffers jxl jxl_threads)

pybind11_add_module(mft_tools ${PROJECT_SOURCE_DIR}/source/python_utilities.cpp)
add_dependencies(mft_tools jxl jxl_threads generate_flatbuffers)
target_link_libraries(mft_tools PRIVATE jxl jxl_threads)