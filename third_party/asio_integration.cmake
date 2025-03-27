add_library(asio_Integrated INTERFACE)
find_package(asio CONFIG REQUIRED)
target_link_libraries(asio_Integrated INTERFACE asio::asio)
target_compile_definitions(asio_Integrated INTERFACE
    "-D_WIN32_WINNT=0x0601")
target_link_libraries(asio_Integrated INTERFACE OpenSSL_Integrated)
