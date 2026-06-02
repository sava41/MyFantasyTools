# libjxl bundles googletest — reuse its targets instead of fetching a second copy.
add_executable(mftest
    test_math.cpp
    test_io.cpp
    test_jxl.cpp
)
target_link_libraries(mftest PRIVATE mflib jxl jxl_threads gtest_main)

add_test(NAME mftest COMMAND mftest)
