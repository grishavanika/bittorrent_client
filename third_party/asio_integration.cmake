include(FetchContent)
include(CMakePrintHelpers)

set(asio_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/asio-src")

FetchContent_Declare(
    asio
    SOURCE_DIR "${asio_SOURCE}"
    FULLY_DISCONNECTED ON)
FetchContent_GetProperties(asio)
if (NOT asio_POPULATED)
    FetchContent_Populate(asio)
endif ()
cmake_print_variables(asio_SOURCE_DIR)

add_library(asio INTERFACE)

target_sources(asio INTERFACE
    "${asio_SOURCE}/asio/include/asio.hpp")
target_include_directories(asio INTERFACE
    "${asio_SOURCE}/asio/include")

target_compile_definitions(asio INTERFACE
    "-D_WIN32_WINNT=0x0601")

if (WIN32)
    target_link_libraries(asio INTERFACE "Ws2_32.lib")
endif ()

if (MSVC)
    target_compile_definitions(asio INTERFACE
        # Say ASIO that MSVC supports coroutines well.
        "-DASIO_HAS_CO_AWAIT"
        "-DASIO_HAS_STD_COROUTINE")

    target_compile_options(asio INTERFACE
        # 'static': an explicit specialization cannot have a storage class (ignored)
        /wd4499
        )
endif ()

if (clang_on_msvc)
    target_compile_options(asio INTERFACE
        -Wno-exit-time-destructors
        -Wno-deprecated-copy-dtor
        -Wno-shadow
        -Wno-implicit-int-conversion
        -Wno-covered-switch-default
        -Wno-nonportable-system-include-path
        -Wno-documentation-unknown-command
        -Wno-shorten-64-to-32
        )
endif ()
