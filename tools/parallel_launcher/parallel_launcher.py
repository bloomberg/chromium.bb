#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This tool launches several shards of a gtest-based binary
in parallel on a local machine.

Example usage:

parallel_launcher.py path/to/base_unittests
"""

import optparse
import os
import subprocess
import sys
import tempfile

class TestLauncher(object):
  def __init__(self, args, executable, num_shards, shard):
    self._args = args
    self._executable = executable
    self._num_shards = num_shards
    self._shard = shard
    self._test = None
    self._tempfile = tempfile.TemporaryFile()

  def launch(self):
    env = os.environ.copy()
    env['GTEST_TOTAL_SHARDS'] = str(self._num_shards)
    env['GTEST_SHARD_INDEX'] = str(self._shard)
    self._test = subprocess.Popen(args=self._args,
                                  executable=self._executable,
                                  stdout=self._tempfile,
                                  stderr=subprocess.STDOUT,
                                  env=env)

  def wait(self):
    code = self._test.wait()
    self._tempfile.seek(0)
    print self._tempfile.read()
    self._tempfile.close()
    return code

def main(argv):
  parser = optparse.OptionParser()
  parser.add_option("--shards", type="int", dest="shards", default=10)

  options, args = parser.parse_args(argv)

  if len(args) != 1:
    print 'You must provide only one argument: path to the test binary'
    return 1

  launchers = []

  for shard in range(options.shards):
    launcher = TestLauncher(args[0], args[0], options.shards, shard)
    launcher.launch()
    launchers.append(launcher)

  return_code = 0
  for launcher in launchers:
    if launcher.wait() != 0:
      return_code = 1

  return return_code

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
