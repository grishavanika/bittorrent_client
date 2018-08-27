
# Download and unpack expected-lite at configure time
configure_file(CMakeLists-expected-lite.txt.in expected-lite-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/expected-lite-download)
if(result)
	message(FATAL_ERROR "CMake step for expected-lite failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/expected-lite-download)
if(result)
	message(FATAL_ERROR "Build step for expected-lite failed: ${result}")
endif()

# Add expected-lite directly to our build
add_library(expected-lite INTERFACE)
target_include_directories(expected-lite INTERFACE
	${CMAKE_BINARY_DIR}/expected-lite-src/include)
