include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(mftest
    test_math.cpp
    test_io.cpp
    test_jxl.cpp
)
target_link_libraries(mftest PRIVATE mflib jxl jxl_threads GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mftest)
