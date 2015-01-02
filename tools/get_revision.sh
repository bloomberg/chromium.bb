#!/bin/bash
# Copyright 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Hepler script to find the current svn revision of the repo that
# works with both svn and git svn checkout.

SCRIPT_DIR="$(dirname "$0")"
SCRIPT_DIR="$(cd "${SCRIPT_DIR}"; pwd)"

set -e
if [ -d ${SCRIPT_DIR}/../.git ]; then
  revision=$(git svn info | grep "Revision: ")
  revision=$(echo $revision | cut -d ' ' -f 2)
  echo $revision
  exit 0
fi

exec svnversion
