
add_custom_target( templates
    SOURCES
        template.packaged.gdextension.in
        template.development.gdextension.in
)

add_dependencies( mft_godot templates )

set( BUILD_TYPE "development")
if (PACKAGED_BUILD)
    set( BUILD_TYPE "packaged")
endif()

set( GD_EXTENSION_FILE_INPUT template.${BUILD_TYPE}.gdextension.in )

# Generate our project's .gdextension file from the template
set( GD_EXTENSION_FILE ${PROJECT_NAME}.gdextension )
configure_file( ${GD_EXTENSION_FILE_INPUT} ${PROJECT_BINARY_DIR}/mft_godot/${GD_EXTENSION_FILE} )

# Install the gdextension file from the build directory
install(
    FILES ${PROJECT_BINARY_DIR}/mft_godot/${GD_EXTENSION_FILE}
    COMPONENT mft
    DESTINATION ${PROJECT_SOURCE_DIR}/mft_godot_plugin
)

unset( BUILD_TYPE )
unset( GD_EXTENSION_FILE )
unset( GD_EXTENSION_FILE_INPUT )
