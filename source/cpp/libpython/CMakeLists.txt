pybind11_add_module(mftools python_utilities.cpp)
add_dependencies(mftools mfutilities)
target_link_libraries(mftools PRIVATE mfutilities)

install(TARGETS mftools COMPONENT mft LIBRARY DESTINATION mft_blender_addon)