include(CMakePrintHelpers)

add_library(OpenSSL_Integrated INTERFACE)
find_package(OpenSSL REQUIRED)
target_link_libraries(OpenSSL_Integrated INTERFACE OpenSSL::SSL)
target_link_libraries(OpenSSL_Integrated INTERFACE OpenSSL::Crypto)

target_compile_options(OpenSSL_Integrated INTERFACE
	/wd4996
	)
