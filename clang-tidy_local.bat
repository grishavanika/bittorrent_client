@echo off

:: WARNING:
:: Requires CMake > version 3.15 (?) with
:: compile_commands.json fix for NMake generator,
:: see https://gitlab.kitware.com/cmake/cmake/issues/17482

set VC_dir="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\"
set LLVM_dir="C:\Programs\LLVM\bin"

set CC=clang-cl
set CXX=clang-cl

set path=%path%;C:\Programs\LLVM\bin
call %VC_dir%vcvars64.bat
mkdir build_clang_nmake
cd build_clang_nmake
cmake -G "NMake Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

:: nmake

clang-tidy.exe -p compile_commands.json ..\\src\\tests\\test_bencoding\\test_be_decode.cpp
clang-tidy.exe -p compile_commands.json ..\\src\\bencoding\\src\\be_element_ref_decoder.cpp
