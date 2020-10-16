include(FetchContent)
include(CMakePrintHelpers)

set(TinySHA1_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/TinySHA1-src")

FetchContent_Declare(
    TinySHA1
    SOURCE_DIR "${TinySHA1_SOURCE}"
    FULLY_DISCONNECTED ON)
FetchContent_GetProperties(TinySHA1)
if (NOT tinysha1_POPULATED)
    FetchContent_Populate(TinySHA1)
endif ()
cmake_print_variables(tinysha1_SOURCE_DIR)

add_library(TinySHA1 INTERFACE)
target_sources(TinySHA1 INTERFACE
	"${TinySHA1_SOURCE}/TinySHA1.hpp")
target_include_directories(TinySHA1 INTERFACE
	"${TinySHA1_SOURCE}")

