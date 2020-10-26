include(CMakePrintHelpers)

# TODO: remove hard-coded path.
# Downloaded from:
# https://slproweb.com/products/Win32OpenSSL.html

if (MSVC)
    # Hard-coded for now, VCPKG fails to build.
    # Don't want to spend time on building this.
    add_library(OpenSSL INTERFACE)
    set(OpenSSLPath "C:/libs/OpenSSL-Win64")
    target_include_directories(OpenSSL INTERFACE
        "${OpenSSLPath}/include")
    target_link_libraries(OpenSSL INTERFACE
        "${OpenSSLPath}/lib/libssl.lib"
        "${OpenSSLPath}/lib/libcrypto.lib")

	cmake_print_variables(OpenSSLPath)
endif ()
