#!/bin/bash
set -exo pipefail

# Copyright 2018-2019 by
# Armin Hasitzka.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.

dir="${PWD}"
cd $( dirname $( readlink -f "${0}" ) ) # go to `/fuzzing/scripts/build'

path_to_freetype=$( readlink -f "../../../external/freetype2" )

if [[ "${#}" == "0" || "${1}" != "--no-init" ]]; then

    # We always want to run the latest version of FreeType:
    git submodule update --init --depth 1 --remote "${path_to_freetype}"

    cd "${path_to_freetype}"

    git clean -dfqx
    git reset --hard
    git rev-parse HEAD

    # Manipulating `ftoption.h' to enable non-standard features of FreeType.
    # There are prettier ways to achieve that, however, this is the simplest:

    git apply "../../fuzzing/settings/freetype2/ftoption.patch"

    sh autogen.sh

    export BZIP2_CFLAGS="-I../bzip2"
    export BZIP2_LIBS="-l../bzip2/libbz2.a"

    export BROTLI_CFLAGS="-I../brotli/c/include"
    export BROTLI_LIBS="-l../brotli/build/libbrotlidec-static.a"

    # Having additional libraries is pain since they have to be linked
    # statically for OSS-Fuzz.  Should additional libraries be required, they
    # have to be linked properly in `fuzzing/src/fuzzers/CMakeLists.txt'.

    sh configure          \
       --enable-static    \
       --disable-shared   \
       --with-brotli      \
       --with-bzip2       \
       --without-harfbuzz \
       --without-png      \
       --without-zlib
fi

cd "${path_to_freetype}"

if [[ -f "Makefile" ]]; then
    make -j$( nproc )
fi

cd "${dir}"
