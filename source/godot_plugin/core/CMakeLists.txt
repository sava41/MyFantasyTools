include(FetchContent)

FetchContent_Declare(
  godot-cpp
  GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
  GIT_TAG godot-4.4.1-stable
)
FetchContent_MakeAvailable(godot-cpp)

add_library( mft_godot SHARED)

target_sources( mft_godot PRIVATE
    gd_manager.h
    gd_manager.cpp
    gd_level.h
    gd_level.cpp
    gd_view_resources.h
    gd_view_resources.cpp
    register_types.cpp
)

target_compile_features( mft_godot PRIVATE cxx_std_17)

# LIB_ARCH is the architecture being built. It is set to the build system's architecture.
# For macOS, we build a universal library (both arm64 and x86_64).
set(LIB_ARCH ${CMAKE_SYSTEM_PROCESSOR})
if (APPLE)
    set(LIB_ARCH "universal")
endif()

if(WIN32)
    set(MFT_PLATFORM "windows")
elseif(APPLE)
    set(MFT_PLATFORM "macos")
else()
    set(MFT_PLATFORM "linux")
endif()

add_dependencies(mft_godot mflib godot-cpp)
target_link_libraries(mft_godot PRIVATE mflib godot-cpp)

if(PACKAGED_BUILD)
    set_target_properties(mft_godot PROPERTIES
        PREFIX ""
        OUTPUT_NAME "libmft_godot.${MFT_PLATFORM}.$<LOWER_CASE:$<CONFIG>>.${LIB_ARCH}"
    )
    install(TARGETS mft_godot COMPONENT mft
        LIBRARY DESTINATION mft_godot_plugin/bin
        RUNTIME DESTINATION mft_godot_plugin/bin)
else()
    install(TARGETS mft_godot COMPONENT mft
        LIBRARY DESTINATION mft_godot_plugin
        RUNTIME DESTINATION mft_godot_plugin)
endif()


