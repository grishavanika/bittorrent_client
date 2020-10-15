set(exe_name u_bittorrent_client)

target_collect_sources(${exe_name})

add_executable(${exe_name} ${${exe_name}_files})

# Enable Unicode Character Set
target_compile_definitions(${exe_name} PRIVATE
    UNICODE _UNICODE)

#if (${gcc})
#    # Make GCC aware about wmain()
#    set_target_properties(${exe_name} PROPERTIES LINK_FLAGS -municode)
#endif()

set_all_warnings(${exe_name} PRIVATE)

target_link_libraries(${exe_name} PRIVATE bencoding)
target_link_libraries(${exe_name} PRIVATE small_utils)

