include(FetchContent)
include(CMakePrintHelpers)

set(CxxUrl_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/CxxUrl-src")

FetchContent_Declare(
	CxxUrl
	SOURCE_DIR "${CxxUrl_SOURCE}"
	FULLY_DISCONNECTED ON)
FetchContent_GetProperties(CxxUrl)
if (NOT cxxurl_POPULATED)
	FetchContent_Populate(CxxUrl)
endif ()
cmake_print_variables(cxxurl_SOURCE_DIR)

set(HEADERS
	${CxxUrl_SOURCE}/url.hpp
    ${CxxUrl_SOURCE}/string.hpp)

set(SOURCES
	${CxxUrl_SOURCE}/url.cpp)

add_library(CxxUrl STATIC ${HEADERS} ${SOURCES})
target_include_directories(CxxUrl PUBLIC "${CxxUrl_SOURCE}")

