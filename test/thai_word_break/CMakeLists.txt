add_executable(ThaiWordBreakTest
  ThaiWordBreakTest.cpp
  ${REPO_ROOT}/lib/ThaiWordBreak/ThaiWordBreak.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(ThaiWordBreakTest PRIVATE
  ${REPO_ROOT}/lib/ThaiWordBreak
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(ThaiWordBreakTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ThaiWordBreakTest)
