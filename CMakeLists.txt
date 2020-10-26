cmake_minimum_required(VERSION 3.12.4 FATAL_ERROR)
# 3.12.4 is required because of C++20 support.
project(bittorrent_client)

# For proper work of CMAKE_EXPORT_COMPILE_COMMANDS.
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

include(tools/cmake/utils.cmake)
include(tools/cmake/clang_msvc_integration.cmake)
include(CTest)

# Enable folders for VS.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# Leave only most Debug/Release configurations.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

detect_compilers()
set_cpp20_standard()

if (clang_on_msvc)
    make_clang_work_on_windows()
endif()

add_subdirectory(third_party)
add_subdirectory(src)
