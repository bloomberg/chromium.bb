#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec_dir=$(dirname $0)
script_dir=${exec_dir}/../../../third_party/WebKit/WebKitTools/Scripts

if [ "$OSTYPE" = "cygwin" ]; then
  system_root=`cygpath "$SYSTEMROOT"`
  PATH="/usr/bin:$system_root/system32:$system_root:$system_root/system32/WBEM"
  export PATH
  PYTHON_PROG="$exec_dir/../../../third_party/python_24/python.exe"
else
  PYTHON_PROG=python
  export PYTHONPATH
fi

"$PYTHON_PROG" "$script_dir/run-chromium-webkit-tests" "$@"
