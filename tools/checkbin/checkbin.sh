#!/bin/sh
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Include pefile in the path.
PYTHONPATH="$(dirname $0)/../../third_party/pefile:$PYTHONPATH"
export PYTHONPATH
python "$(dirname $0)/checkbin.py" "$@"
