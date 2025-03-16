
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