#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The script is located in "pnacl/", the same
# directory as build.sh.
PNACL_DIR="$(dirname "$0")"
LIBMODE=glibc "${PNACL_DIR}"/build.sh "$@"
