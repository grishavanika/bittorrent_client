@echo off

set VC_dir="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\"
set LLVM_dir="C:\Programs\LLVM\bin"

set CC=clang-cl
set CXX=clang-cl

set path=%path%;C:\Programs\LLVM\bin
call %VC_dir%vcvars64.bat
mkdir build_clang_nmake
cd build_clang_nmake
cmake -G "NMake Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
nmake

:: clang-tidy.exe -p C:\dev\bencoding\build_gcc_x64\compile_commands.json C:\dev\bencoding\src\tests\test_bencoding\test_be_decode.cpp
