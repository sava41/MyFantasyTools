
include(FetchContent)

message("Configuring flatbuffers")
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v23.5.26
)

set(FLATBUFFERS_BUILD_TESTS OFF)
FetchContent_MakeAvailable(flatbuffers)
