add_library(GTest_Integrated INTERFACE)
find_package(GTest CONFIG REQUIRED)
target_link_libraries(GTest_Integrated INTERFACE
    GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
