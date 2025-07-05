
include(FetchContent)

message("Configuring imath")
FetchContent_Declare(
  imath
  GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/Imath
  GIT_TAG v3.1.12
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

FetchContent_GetProperties(imath)
if(NOT imath_POPULATED)
  FetchContent_Populate(imath)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  
  add_subdirectory(${imath_SOURCE_DIR} ${imath_BINARY_DIR})
endif()