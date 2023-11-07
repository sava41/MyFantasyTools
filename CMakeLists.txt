cmake_minimum_required(VERSION 3.19)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

project(MyFantasyTools)

#-------Set Up Third Party Dependencies-------
set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/third_party)

message("Downloading libjxl")
FetchContent_Declare(
  libjxl
  GIT_REPOSITORY https://github.com/libjxl/libjxl.git
  GIT_TAG v0.8.2
)
set(JPEGXL_STATIC true)
set(JPEGXL_ENABLE_TOOLS true)
FetchContent_GetProperties(libjxl)
if(NOT libjxl_POPULATED)
  FetchContent_Populate(libjxl)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  add_subdirectory(${libjxl_SOURCE_DIR} ${libjxl_BINARY_DIR})
endif()

#FetchContent_MakeAvailable(libjxl)

message("Downloading flatbuffers")
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

#-------Compile MFT Tools and Libraries-------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

file(GLOB_RECURSE MFT_SRCS	
	"${PROJECT_SOURCE_DIR}/source/*.cpp"
	"${PROJECT_SOURCE_DIR}/source/*.h")

# Use flatc to generate files
set(FBS_FILES ${CMAKE_SOURCE_DIR}/source/flatbuffers/level.fbs)
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated_flatbuffers)
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
  OUTPUT ${GENERATED_DIR}/level_generated.h
  COMMAND flatc ARGS --cpp --python -o ${GENERATED_DIR} ${FBS_FILES}
  DEPENDS flatc ${FBS_FILES}
  COMMENT "Generating FlatBuffers headers from ${FBS_FILES}"
)

add_executable(MyFantasyTools ${MFT_SRCS} ${GENERATED_DIR}/level_generated.h)
add_dependencies(MyFantasyTools flatc jxl jxl_threads cjxl)
target_include_directories(MyFantasyTools PRIVATE ${GENERATED_DIR})
target_link_libraries(MyFantasyTools PRIVATE flatbuffers jxl jxl_threads)