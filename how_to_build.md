git submodule update --init --recursive
cmake -DCMAKE_TOOLCHAIN_FILE=c:\libs\vcpkg\scripts\buildsystems\vcpkg.cmake -A x64 -H. -Bbuild
cmake -G "Visual Studio 16 2019" -A x64 -T ClangCL -H. -Bbuild_clang
set path=%path%;C:\Programs\MinGW\bin
cmake -G "MinGW Makefiles" -H. -Bbuild_mingw
