#!/bin/bash
#
# Copyright (c) 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is wrapper around arm-linux-gnueabi-gcc/g++ so we can easily sneak
# in some extra flags required for building chrome using the nacl
# arm trusted TC.
# This script will be copied into the the arm trusted TC jail
# and changes behavior slightly depending on what name is used to invoke it.

set -o nounset
set -o errexit

readonly JAIL=$(dirname $0)

if [[ $(basename $0) == *c++* ]] ; then
  readonly COMPILER=arm-linux-gnueabi-g++-4.5
else
  readonly COMPILER=arm-linux-gnueabi-gcc-4.5
fi

# NOTE: the glib-2.0  gdk-pixbuf-2.0  should be inferred via pkg-config
#       but something is not quite right yet with jail setup

${COMPILER} \
 -isystem ${JAIL}/usr/include \
 -I${JAIL}/usr/lib/arm-linux-gnueabi/glib-2.0/include \
 -I${JAIL}/usr/include/gdk-pixbuf-2.0 \
 -Wl,-rpath-link=${JAIL}/usr/lib/arm-linux-gnueabi \
 -Wl,-rpath-link=${JAIL}/usr/lib \
 -Wl,-rpath-link=${JAIL}/lib/arm-linux-gnueabi \
 -L${JAIL}/lib \
 -L${JAIL}/usr/lib \
 -L${JAIL}/lib/arm-linux-gnueabi \
 -L${JAIL}/usr/lib/arm-linux-gnueabi \
 "$@"
