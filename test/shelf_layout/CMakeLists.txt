add_executable(ShelfLayoutTest ShelfLayoutTest.cpp)

target_include_directories(ShelfLayoutTest PRIVATE ../../src)

target_link_libraries(ShelfLayoutTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ShelfLayoutTest)
