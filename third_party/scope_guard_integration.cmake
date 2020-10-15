include(FetchContent)

FetchContent_Declare(
    scope_guard_Content
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/scope_guard-src"
    FULLY_DISCONNECTED ON)
FetchContent_GetProperties(scope_guard_Content)
if (NOT scope_guard_Content_POPULATED)
    FetchContent_Populate(scope_guard_Content)
endif ()

add_library(scope_guard INTERFACE)
target_sources(scope_guard INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/scope_guard-src/ScopeGuard.h")
target_include_directories(scope_guard INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/scope_guard-src")

