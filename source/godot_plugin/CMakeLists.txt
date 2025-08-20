include(FetchContent)

FetchContent_Declare(
  godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG godot-4.4.1-stable
)
FetchContent_MakeAvailable(godot-cpp)

add_subdirectory(core)