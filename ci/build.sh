#!/bin/sh -x
# Build the simulator
set -e

mkdir -p build
cd build
cmake ..
make
make box
cd ..