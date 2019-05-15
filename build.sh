#!/bin/bash

mkdir -p build
mkdir -p ../libmass/build

pushd ../libmass/build
cmake ..
make -j4
popd

pushd build
cmake ..
make -j4
popd
