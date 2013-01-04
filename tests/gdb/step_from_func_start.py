# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  gdb.Command('break test_step_from_function_start')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  gdb.ResumeCommand('step')
  gdb.ResumeCommand('step')
  gdb.ResumeCommand('step')
  AssertEquals(gdb.Eval('global_var'), '1')
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'step_from_func_start')
