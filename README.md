# Bencoding

[![Build Status](https://ci.appveyor.com/api/projects/status/54skmjik5tmfecpy?svg=true)](https://ci.appveyor.com/project/grishavanika/bencoding)

### LLVM on Windows (VS2019)

 * Go to <https://llvm.org/builds>.
 * Download & install Windows installer.
   Currently, it's <https://prereleases.llvm.org/win-snapshots/LLVM-11.0.0-2663a25f-win64.exe>.
 * Download & install "LLVM Compiler Toolchain Visual Studio extension"
   for VS **2019** from <https://marketplace.visualstudio.com/items?itemName=MarekAniola.mangh-llvm2019>.  
   With this you can do `cmake -G "Visual Studio 16 2019" -A x64 -T LLVM ..`.
 * OR otherwise, use built-in ClangCL support, see
   <https://docs.microsoft.com/en-us/cpp/build/clang-support-msbuild?view=vs-2019>.  
   Once needed VS components installed, you can do `cmake -G "Visual Studio 16 2019" -A x64 -T ClangCL ..`.
 * See this CMake issue for more details: <https://gitlab.kitware.com/cmake/cmake/issues/19174>.
