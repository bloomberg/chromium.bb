#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec_dir=$(dirname $0)

if [ "$OSTYPE" = "cygwin" ]; then
  system_root=`cygpath "$SYSTEMROOT"`
  PATH="/usr/bin:$system_root/system32:$system_root:$system_root/system32/WBEM"
  export PATH
  unset PYTHONPATH
  PYTHON_PROG="$exec_dir/../../../third_party/python_24/python.exe"
  SCRIPT=$(cygpath -wa "$exec_dir/run_http_server.py")
else
  PYTHON_PROG=python
  unset PYTHONPATH
  SCRIPT="$exec_dir/run_http_server.py"
fi

"$PYTHON_PROG" "$SCRIPT" "$@"
