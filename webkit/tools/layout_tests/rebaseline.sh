#!/bin/sh

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec_dir=$(dirname $0)

if grep -q REBASELINE $exec_dir/test_expectations.txt ; then
  echo "test_expectations.txt has been moved upstream! The test_expectations.txt"
  echo "here is just a set of local overrides. You have to put the REBASELINE "
  echo "tags in the upstream version and commit the new baselines into the WebKit"
  echo "tree."
  echo
  echo "Try:"
  echo "cd ../../../third_party/WebKit"
  echo "\$EDITOR LayoutTests/platform/chromium/test_expectations.txt"
  echo "./Tools/Scripts/rebaseline-chromium-webkit-tests"
  echo
  echo "See also https://trac.webkit.org/wiki/Rebaseline"
  exit 1
fi

PYTHON_PROG=python

"$PYTHON_PROG" "$exec_dir/../../../third_party/WebKit/Tools/Scripts/rebaseline-chromium-webkit-tests" "$@"
