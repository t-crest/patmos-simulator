#!/bin/sh -x
# Run automatic tests
set -e

cd build
ctest --verbose --output-on-failure
cd ..