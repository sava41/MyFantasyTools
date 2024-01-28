pybind11_add_module(mftools python_utilities.cpp)
add_dependencies(mftools jxl-static jxl_threads-static generate_flatbuffers)
target_link_libraries(mftools PRIVATE jxl-static jxl_threads-static)