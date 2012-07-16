#!/bin/bash
#
# Copyright (c) 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is wrapper around gcc/g++ so we can easily sneak in some extra
# flags required for building chrome using the nacl arm trusted TC.
# This script will be copied into the the arm trusted TC jail
# and changes behavior slightly depending on what name is used to invoke it.

# NOTE: this script should not be necessary, c.f. below

set -o nounset
set -o errexit

if [[ $(basename $0) == *c++* ]] ; then
  readonly COMPILER=g++
else
  readonly COMPILER=gcc
fi

# NOTE: the host compiler seems to be invoked with flags from the
#       target compiler which is wrong. We add the hacks below to work
#       around this

${COMPILER} -I/usr/lib/glib-2.0/include "$@"
