
include(FetchContent)

message("Configuring openexr")
FetchContent_Declare(
  openexr
  GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openexr
  GIT_TAG v3.3.4
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(OPENEXR_BUILD_TOOLS OFF CACHE BOOL "Build OpenEXR tools" FORCE)
set(OPENEXR_INSTALL_TOOLS OFF CACHE BOOL "Install OpenEXR tools" FORCE)
set(OPENEXR_BUILD_EXAMPLES OFF CACHE BOOL "Build OpenEXR examples" FORCE)
set(OPENEXR_INSTALL_EXAMPLES OFF CACHE BOOL "Install OpenEXR examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build tests" FORCE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  
FetchContent_MakeAvailable(openexr)