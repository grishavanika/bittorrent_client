include(FetchContent)
include(CMakePrintHelpers)

set(leaf_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/leaf-src")

FetchContent_Declare(
    leaf
    SOURCE_DIR "${leaf_SOURCE}"
    FULLY_DISCONNECTED ON)
FetchContent_GetProperties(leaf)
if (NOT leaf_POPULATED)
    FetchContent_Populate(leaf)
endif ()
cmake_print_variables(leaf_SOURCE_DIR)

add_library(leaf INTERFACE)

target_sources(leaf INTERFACE
    "${leaf_SOURCE}/include/boost/leaf.hpp")
target_include_directories(asio INTERFACE
    "${leaf_SOURCE}/include/boost")
