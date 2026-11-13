add_executable(ThaiCalendarTest ThaiCalendarTest.cpp ${REPO_ROOT}/src/util/ThaiCalendar.cpp)
target_include_directories(ThaiCalendarTest PRIVATE ${REPO_ROOT}/src)
target_link_libraries(ThaiCalendarTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(ThaiCalendarTest)
