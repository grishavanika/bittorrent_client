#!/bin/bash
#rm -rf build_nix
mkdir build_nix & cd  build_nix
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
ctest --verbose
cd ..

