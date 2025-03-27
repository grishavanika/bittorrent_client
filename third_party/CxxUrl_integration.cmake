include(FetchContent)
include(CMakePrintHelpers)

FetchContent_Declare(
    cxxurl
    GIT_REPOSITORY https://github.com/chmike/CxxUrl.git
    GIT_TAG        eaf46c0207df24853a238d4499e7f4426d9d234c
    )
FetchContent_MakeAvailable(cxxurl)
cmake_print_variables(cxxurl_SOURCE_DIR)

add_library(CxxUrl_Integrated INTERFACE)
target_link_libraries(CxxUrl_Integrated INTERFACE chmike::CxxUrl)
