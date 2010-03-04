#!/bin/sh

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec_dir=$(dirname $0)

PYTHON_PROG=python

"$PYTHON_PROG" "$exec_dir/../../../third_party/WebKit/WebKitTools/Scripts/rebaseline-chromium-webkit-tests" "$@"
