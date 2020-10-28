set(exe_name bittorrent_client)

target_collect_sources(${exe_name})

add_executable(${exe_name} ${${exe_name}_files})

set_all_warnings(${exe_name} PRIVATE)

target_link_libraries(${exe_name} PRIVATE bencoding)
target_link_libraries(${exe_name} PRIVATE small_utils)
target_link_libraries(${exe_name} PRIVATE CxxUrl)
target_link_libraries(${exe_name} PRIVATE asio)
