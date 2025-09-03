add_subdirectory(core)
add_subdirectory(templates)

file(GLOB MFT_PLUGIN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/*")
install(FILES ${MFT_PLUGIN_FILES} COMPONENT mft DESTINATION ${PROJECT_SOURCE_DIR}/mft_godot_plugin)

