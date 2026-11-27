add_executable(ReadingHistoryTest ReadingHistoryTest.cpp)

target_include_directories(ReadingHistoryTest PRIVATE ../../src)

target_link_libraries(ReadingHistoryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingHistoryTest)
