# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break foo')
  gdb.Command('break bar')
  # Program runs 2 threads, each calls foo and bar - expect 4 breakpoint hits.
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'break_continue_thread', os.environ['GDB_TEST_GUEST'])
