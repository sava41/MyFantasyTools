
include(FetchContent)

# Fix atomic operations detection on macOS with Ninja
if(APPLE)
    set(ATOMICS_FOUND TRUE)
    set(ATOMICS_LIBRARIES "")
    set(ATOMICS_LOCK_FREE_INSTRUCTIONS TRUE)
endif()

message("Configuring libjxl")
FetchContent_Declare(
  libjxl
  GIT_REPOSITORY https://github.com/libjxl/libjxl.git
  GIT_TAG v0.11.1
)
set(JPEGXL_STATIC true)
set(JPEGXL_ENABLE_TOOLS OFF)
set(JPEGXL_ENABLE_MANPAGES OFF)
set(JPEGXL_ENABLE_EXAMPLES OFF)
set(JPEGXL_ENABLE_BENCHMARK OFF)
set(JPEGXL_ENABLE_SJPEG OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

FetchContent_MakeAvailable(libjxl)