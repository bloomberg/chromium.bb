#!/bin/bash
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.
#
# This script is tested ONLY on Linux. It may not work correctly on
# Mac OS X.
#

if [ $# -lt 1 ];
then
  echo "Usage: "$0" (android|android_small|cast|chromeos|common|flutter|ios)" >&2
  exit 1
fi

TOPSRC="$(dirname "$0")/.."
ICU_DATA_FILTER_FILE="${TOPSRC}/filters/$1.json"
ICU_DATA_FILTER_FILE="${ICU_DATA_FILTER_FILE}" "${TOPSRC}/source/runConfigureICU" --enable-debug --disable-release Linux/gcc --disable-tests
