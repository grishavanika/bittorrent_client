set(lib_name small_utils)

target_collect_sources(${lib_name})

add_library(${lib_name} ${${lib_name}_files})

set_all_warnings(${lib_name} PUBLIC)

if (clang_on_msvc)
    target_compile_options(small_utils PUBLIC
        -Wno-unsafe-buffer-usage)
endif()

target_include_directories(${lib_name} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${lib_name} PUBLIC include)

target_link_libraries(${lib_name} PUBLIC TinySHA1_Integrated)

