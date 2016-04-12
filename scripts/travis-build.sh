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

build() {
  ./configure
  make -j${JOBS} check
}

main() {
  setup_env
  build
}

main "$@"
