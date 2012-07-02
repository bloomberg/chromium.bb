# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break main')
  gdb.ResumeCommand('continue')
  gdb.ResumeCommand('stepi')
  gdb.Quit()
  return 0

if __name__ == '__main__':
  gdb_test.RunTest(test, 'stepi_after_break', os.environ['GDB_TEST_GUEST'])

