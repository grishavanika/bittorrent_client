include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(test_name bencoding)

add_subdirectory(test_${test_name})
set_target_properties(test_${test_name} PROPERTIES FOLDER tests)
new_test(${test_name} test_${test_name})
