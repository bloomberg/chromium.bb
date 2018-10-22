#!/bin/bash
#
# Build file to set up and run tests

# Change to repo root
cd $(dirname $0)/../../..

git submodule update --init --recursive
bazel test :protobuf_test
