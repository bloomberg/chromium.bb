#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The script is located in "pnacl/".
# Set pwd to pnacl/
cd "$(dirname "$0")"
LIBMODE=glibc ./build.sh "$@"
