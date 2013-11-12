#!/bin/bash
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Downloads, builds (with instrumentation) and installs shared libraries.

# Go to this shell script directory
cd $(dirname "$0")

# Colored print.

RED_COLOR='\e[0;31m'
GREEN_COLOR='\e[0;32m'
NO_COLOR='\e[0m'

function echo_red {
  echo -e "${RED_COLOR}$1${NO_COLOR}"
}

function echo_green {
  echo -e "${GREEN_COLOR}$1${NO_COLOR}"
}

# Default values.

product_dir=$(pwd)
intermediate_dir=$(pwd)

# Should be without spaces to pass it to dpkg-buildpackage.
makejobs="-j14"

# Parsing args.

while getopts ":i:l:m:hp:s:j:" opt; do
  case ${opt} in
    p)
      echo_green "Only installing dependencies (requires root access)"
      echo_green "Installing dependencies for: ${OPTARG}"
      sudo apt-get -y --no-remove build-dep ${OPTARG}
      exit
      ;;
    h)
      echo "Possible flags:
        -p - install dependencies for packages,
        -h - this help,
        -l - which library to build,
        -i - sets relative product_dir,
        -s - sanitizer (only asan is supported now)."
      echo "Environment variabless, which affect this script: CC and CXX"
      exit
      ;;
    i)
      product_dir="$(pwd)/${OPTARG}"
      ;;
    l)
      library="${OPTARG}"
      ;;
    m)
      intermediate_dir="${OPTARG}"
      ;;
    j)
      makejobs="-j${OPTARG}"
      ;;
    s)
      sanitizer_type="${OPTARG}"
      if [[ "${OPTARG}" == "asan" ]]; then
        sanitizer_flag_string="address"
      else
        echo_red "Invalid sanitizer: ${OPTARG}" >&2
        exit 1
      fi
      ;;
    *)
      echo_red "Invalid option: -${OPTARG}" >&2
      exit 1
      ;;
  esac
done

if [[ -z "${library}" ]]; then
  echo_red "No library specified to build" >&2
  exit 1
fi

if [[ -z "${sanitizer_type}" ]]; then
  echo_red "No sanitizer specified" >&2
  exit
fi

export CFLAGS="-fsanitize=${sanitizer_flag_string} -g -fPIC -w"
export CXXFLAGS="-fsanitize=${sanitizer_flag_string} -g -fPIC -w"

# We use XORIGIN as RPATH and after building library replace it to $ORIGIN
# The reason: this flag goes through configure script and makefiles
# differently for different libraries. So the dollar sign "$" should be
# differently escaped. Instead of having problems with that it just
# uses XORIGIN to build library and after that replaces it to $ORIGIN
# directly in .so file.
export LDFLAGS="-Wl,-z,origin -Wl,-R,XORIGIN/."

mkdir -p ${product_dir}/instrumented_libraries/${sanitizer_type}

needed_dependencies=$(apt-get -s build-dep ${library} | grep Inst \
  | cut -d " " -f 2)

if [[ -n "${needed_dependencies}" ]]; then
  echo_red "Library ${library} needs dependencies: ${needed_dependencies}" >&2
  echo_red "Please, install dependencies using:
    third_party/instrumented_libraries/download_build_install -p ${library}" >&2
  exit 1
fi

(
  # Downloading library
  mkdir -p ${intermediate_dir}/${library}
  cd ${intermediate_dir}/${library}
  apt-get source ${library} 2>&1

  # cd into the only directory in the current folder
  # where our package has been unpacked.
  cd $(ls -F |grep \/$)

  # Build library
  ./configure --prefix="${product_dir}/instrumented_libraries/${sanitizer_type}"
  make ${makejobs}

  # Install library
  make ${makejobs} install 2>&1
) > /dev/null

# Mark that library is installed.
# This file is used by GYP as 'output' to mark that it's already build
# while making incremental build.
touch ${product_dir}/instrumented_libraries/${sanitizer_type}/${library}.txt

# Clean up.
rm -rf ${intermediate_dir}/${library}
