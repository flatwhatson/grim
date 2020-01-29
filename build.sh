#!/bin/bash

cd $(dirname $(readlink -f $0))

[[ $1 == "clean" ]] && rm -rf build
mkdir -p build
cd build

cmake .. -G Ninja
ninja -v
