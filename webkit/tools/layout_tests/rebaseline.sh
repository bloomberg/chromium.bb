#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec_dir=$(dirname $0)

PYTHON_PROG=python
# When not using the included python, we don't get automatic site.py paths.
# Specifically, rebaseline.py needs the paths in:
# third_party/python_24/Lib/site-packages/google.pth
PYTHONPATH="${exec_dir}/../../../tools/python:$PYTHONPATH"
export PYTHONPATH

"$PYTHON_PROG" "$exec_dir/rebaseline.py" "$@"
