#!/usr/bin/env bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(thakis): Remove this script once Source/config.gyp refers the the .py
# file.
THIS_DIR="$(dirname "${0}")"
ABS_THIS_DIR="${PWD}/${THIS_DIR}"
exec python "${ABS_THIS_DIR}/blink_gc_plugin_flags.py" "$@"
