cmake_minimum_required(VERSION 3.22)
project(MyFantasyTools)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_BLENDER_PLUGIN "Build Blender plugin" ON)
option(BUILD_GODOT_PLUGIN "Build Godot plugin" ON)

if(BUILD_BLENDER_PLUGIN)
    find_package(Python 3.11 EXACT COMPONENTS Interpreter Development REQUIRED)
endif()

set(FETCHCONTENT_BASE_DIR ${CMAKE_BINARY_DIR}/external)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

if(APPLE)
    set(CMAKE_THREAD_LIBS_INIT "-lpthread")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 0)
    set(CMAKE_USE_PTHREADS_INIT 1)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()

add_subdirectory(source/core)

if(BUILD_BLENDER_PLUGIN)
    add_subdirectory(source/blender_plugin)
endif()

if(BUILD_GODOT_PLUGIN)
    add_subdirectory(source/godot_plugin)
endif()