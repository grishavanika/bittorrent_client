include(FetchContent)
include(CMakePrintHelpers)

FetchContent_Declare(
    TinySHA1
    GIT_REPOSITORY https://github.com/mohaps/TinySHA1.git
    GIT_TAG        2795aa8de91b1797defdfbff61ed93b22b5ced81
    )
FetchContent_MakeAvailable(TinySHA1)
cmake_print_variables(tinysha1_SOURCE_DIR)

add_library(TinySHA1_Integrated INTERFACE)
target_sources(TinySHA1_Integrated INTERFACE
	"${tinysha1_SOURCE_DIR}/TinySHA1.hpp")
target_include_directories(TinySHA1_Integrated INTERFACE
	"${tinysha1_SOURCE_DIR}")
