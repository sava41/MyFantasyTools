
add_subdirectory(external/flatbuffers)
add_subdirectory(external/libjxl)

add_subdirectory(flatbuffers)
add_subdirectory(utilities)
add_subdirectory(libmft)

if(BUILD_BLENDER_PLUGIN)
    add_subdirectory(external/pybind)
    add_subdirectory(external/openexr)
    add_subdirectory(external/imath)
    add_subdirectory(libpython)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()