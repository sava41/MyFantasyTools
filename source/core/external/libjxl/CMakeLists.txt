
include(FetchContent)

message("Configuring libjxl")
FetchContent_Declare(
  libjxl
  GIT_REPOSITORY https://github.com/libjxl/libjxl.git
  GIT_TAG v0.9.2
)
set(JPEGXL_STATIC true)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
FetchContent_MakeAvailable(libjxl)