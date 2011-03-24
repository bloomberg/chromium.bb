#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file pools the appspot directory for a fixed time.
# TODO(khim): invetigate feasibility of buildbot triggering.

REVISION="$("$(dirname "$0")/glibc_revision.sh")"
for ((i=1;i<100;i+=i)); do
  curl --fail --location --url http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$REVISION"/glibc_x86.tar.gz -o "$1/.glibc.tar" &&
  tar xSvpf "$1/.glibc.tar" -C "$1" &&
  exit 0
  for ((j=1;(j*10)<i;j++)); do
    echo "Check if revision "$((REVISION+j))" is available..."
    if curl --fail --location --url http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$((REVISION+j))"/glibc_x86.tar.gz > /dev/null; then
      exit 2
    fi
    sleep 10
  done
done
exit 1
