# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess

# TODO(timurrrr): we may use it on POSIX too to avoid code duplication once we
# support layout_tests, remove Dr. Memory specific code and verify it works
# on a "clean" Mac.

wrapper_pid = os.getpid()
testcase_name = None
for arg in sys.argv:
  m = re.match("\-\-test\-name=(.*)", arg)
  if m:
    assert testcase_name is None
    testcase_name = m.groups()[0]

# arg #0 is the path to this python script
cmd_to_run = sys.argv[1:]

# TODO(timurrrr): this is Dr. Memory-specific
# Usually, we pass "-logdir" "foo\bar\spam path" args to Dr. Memory.
# To group reports per UI test, we want to put the reports for each test into a
# separate directory. This code can be simplified when we have
# http://code.google.com/p/drmemory/issues/detail?id=684 fixed.
logdir_idx = cmd_to_run.index("-logdir")
old_logdir = cmd_to_run[logdir_idx + 1]
cmd_to_run[logdir_idx + 1] += "\\testcase.%d.logs" % wrapper_pid
os.makedirs(cmd_to_run[logdir_idx + 1])

if testcase_name:
  f = open(old_logdir + "\\testcase.%d.name" % wrapper_pid, "w")
  print >>f, testcase_name
  f.close()

exit(subprocess.call(cmd_to_run))
