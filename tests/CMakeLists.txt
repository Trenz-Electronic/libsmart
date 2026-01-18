add_executable(test_smart
    test_string.cpp
    test_path.cpp
    test_circular_buffer.cpp
)
target_link_libraries(test_smart PRIVATE smart crack crypt Catch2::Catch2WithMain)
target_include_directories(test_smart PRIVATE ${CMAKE_SOURCE_DIR})

include(CTest)
include(Catch)
catch_discover_tests(test_smart)
