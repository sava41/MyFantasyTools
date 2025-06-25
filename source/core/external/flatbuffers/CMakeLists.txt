
include(FetchContent)

message("Configuring flatbuffers")
FetchContent_Declare(
  flatbuffers
  GIT_REPOSITORY https://github.com/google/flatbuffers.git
  GIT_TAG v23.5.26
)

FetchContent_MakeAvailable(flatbuffers)
