# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break test_stepi_after_break')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  # From GDB/MI documentation, 'stepi' statement should be
  #   assert gdb.ResumeCommand('stepi')['reason'] == 'end-stepping-range'
  # but in reality 'stepi' stop reason is simply omitted.
  gdb.ResumeCommand('stepi')
  gdb.Quit()
  return 0

if __name__ == '__main__':
  gdb_test.RunTest(test, 'stepi_after_break', os.environ['GDB_TEST_GUEST'])
