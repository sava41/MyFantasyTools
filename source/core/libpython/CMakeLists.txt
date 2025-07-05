pybind11_add_module(mftools python_utilities.cpp)
add_dependencies(mftools mfutilities OpenEXR::OpenEXR Imath::Imath)
target_link_libraries(mftools PRIVATE mfutilities OpenEXR::OpenEXR Imath::Imath)

install(TARGETS mftools COMPONENT mft LIBRARY DESTINATION ${PROJECT_SOURCE_DIR}/mft_blender_addon)