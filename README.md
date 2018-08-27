# Bencoding

[![Build Status (Windows)](https://ci.appveyor.com/api/projects/status/5c1u1pjffcfe5i9x?svg=true)](https://ci.appveyor.com/project/grishavanika/cpp-initial-cmake)
[![Build Status (Linux)](https://travis-ci.org/grishavanika/cpp_initial_cmake.svg)](https://travis-ci.org/grishavanika/cpp_initial_cmake)

# Supported compilers

1. Windows:
	- MSVC (tested with MSVC 2017)
	- Clang (tested with integration to MSVC 2017)
	- GCC
2. Non-Windows:
	- GCC
	- Clang

# How-to

1. With only CMake:
	```
	mkdir build && cd build
	cmake -G "Visual Studio 15 2017 Win64" ..
	# cmake ..
	cmake --build . --config Release
	```

2. With Python help:
- Go to `tools` folder
- `python generate_project.py --compiler=gcc`
- Go to needed build_gcc_x64 folder in the root of project
	(depends on `generate_project` options)
- `cmake --build . --config Release`

# Requirements

1. On Windows
	- (If needed) MSVC 2017
	- (If needed) GCC (for example: https://nuwen.net/mingw.html)
	- (If needed) Clang (binaries: http://llvm.org/builds)
2. On non-Windows platforms:
	- Whatever CMake supports
3. (Optionally) Python to run helper `generate_project` script
	if you don't want to remember/type CMake's options

# Notes for Windows and non-MSVC compilers

Clang integration into MSVC 2017 is a bit broken (everything ok for MSVC 2015):

- see this bug-report for more details: https://gitlab.kitware.com/cmake/cmake/issues/17930
- see this LLVM report for tracking the status: https://bugs.llvm.org/show_bug.cgi?id=33672
- see this GitHub project for possible fix: https://github.com/Farwaykorse/VS_Clang

# Out-of-box support
- Google Test
- MSVC 2017 (see "Supported compilers" section)
- AppVeyor integration
- Travis CI integration
