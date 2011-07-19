#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd "$(dirname "$0")"
for i in binutils gcc gdb glibc linux-headers-for-nacl newlib; do
  (
    if [ -s "git-try.$i.patch" ]; then
      make "pinned-src-$i"
      cd "SRC/$i"
      patch -p1 < ../../"git-try.$i.patch"
    fi
  )
done
