include(FetchContent)

FetchContent_Declare(
	expected_lite
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/expected-lite-src"
	FULLY_DISCONNECTED ON)
FetchContent_GetProperties(expected_lite)
if (NOT expected_lite_POPULATED)
	FetchContent_Populate(expected_lite)
endif ()

add_subdirectory(${expected_lite_SOURCE_DIR} ${expected_lite_BINARY_DIR} EXCLUDE_FROM_ALL)
