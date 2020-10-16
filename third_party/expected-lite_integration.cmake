include(FetchContent)
include(CMakePrintHelpers)

FetchContent_Declare(
	expected_lite
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/expected-lite-src"
	FULLY_DISCONNECTED ON)
FetchContent_GetProperties(expected_lite)
if (NOT expected_lite_POPULATED)
	FetchContent_Populate(expected_lite)
endif ()
cmake_print_variables(expected_lite_SOURCE_DIR)

add_subdirectory(${expected_lite_SOURCE_DIR} ${expected_lite_BINARY_DIR} EXCLUDE_FROM_ALL)

if (clang_on_msvc)
	target_compile_options(expected-lite INTERFACE
		-Wno-missing-noreturn)
endif ()
