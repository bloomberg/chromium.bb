#!/bin/sh

set -ex

setup_env() {
  # Travis sets CC/CXX to the system toolchain, so our .travis.yml
  # exports USE_{CC,CXX} for this script to use.
  if [ -n "$USE_CC" ]; then
      export CC=$USE_CC
  fi
  if [ -n "$USE_CXX" ]; then
      export CXX=$USE_CXX
  fi
  # Use -jN for faster builds. Travis build machines under Docker
  # have a lot of cores, but are memory-limited, so the kernel
  # will OOM if we try to use them all, so use at most 4.
  # See https://github.com/travis-ci/travis-ci/issues/1972
  export NCPUS=$(getconf _NPROCESSORS_ONLN)
  export JOBS=$(( $NCPUS < 4 ? $NCPUS : 4 ))
}

# Do an in-tree build and make sure tests pass.
build() {
  ./configure
  make -j${JOBS} check VERBOSE=1
  make distclean
}

# Do an out-of-tree build and make sure we can create a release tarball.
build_out_of_tree() {
  mkdir -p build/native
  cd build/native
  ../../configure
  make -j${JOBS} distcheck VERBOSE=1
}

main() {
  setup_env
  build
  build_out_of_tree
}

main "$@"
