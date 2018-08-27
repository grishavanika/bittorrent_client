# expected-lite integration

add_subdirectory(expected-lite)

if (clang_on_msvc)
	target_compile_options(expected-lite INTERFACE
		-Wno-missing-noreturn)
endif()
