# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  gdb.Command('break test_change_variable')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  gdb.Command('set var arg = 2')
  AssertEquals(gdb.Eval('arg'), '2')
  gdb.ResumeCommand('step')
  AssertEquals(gdb.Eval('local_var'), '4')
  gdb.Command('set var local_var = 5')
  AssertEquals(gdb.Eval('local_var'), '5')
  gdb.Command('set var global_var = 1')
  AssertEquals(gdb.Eval('global_var'), '1')
  gdb.ResumeCommand('step')
  AssertEquals(gdb.Eval('global_var'), '8') # 1 + 5 + 2


if __name__ == '__main__':
  gdb_test.RunTest(test, 'change_variable')
