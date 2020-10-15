include(FetchContent)

FetchContent_Declare(
    TinySHA1_Content
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/TinySHA1-src"
    FULLY_DISCONNECTED ON)
FetchContent_GetProperties(TinySHA1_Content)
if (NOT TinySHA1_Content_POPULATED)
    FetchContent_Populate(TinySHA1_Content)
endif ()

add_library(TinySHA1 INTERFACE)
target_sources(TinySHA1 INTERFACE
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/TinySHA1-src/TinySHA1.hpp")
target_include_directories(TinySHA1 INTERFACE
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/TinySHA1-src")

