#!/bin/bash

# Fail on any error.
set -e
# Display commands being run.
set -x

cd git/SwiftShader

git submodule update --init

mkdir -p build && cd build

if [[ -z "${REACTOR_BACKEND}" ]]; then
  REACTOR_BACKEND="LLVM"
fi

cmake .. "-DASAN=ON -DREACTOR_BACKEND=${REACTOR_BACKEND} -DCMAKE_BUILD_TYPE=RelWithDebInfo"
make -j$(sysctl -n hw.logicalcpu)

# Run the reactor unit tests.
./ReactorUnitTests

cd .. # Tests must be run from project root

# Run the OpenGL ES and Vulkan unit tests.
build/gles-unittests

if [ "${REACTOR_BACKEND}" != "Subzero" ]; then
  # Currently vulkan does not work with Subzero.
  build/vk-unittests
fi
