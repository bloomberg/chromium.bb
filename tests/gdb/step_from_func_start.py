# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break test_step_from_function_start')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  gdb.ResumeCommand('step')
  gdb.ResumeCommand('step')
  #assert gdb.Eval('global_var') == '0'
  gdb.ResumeCommand('step')
  assert gdb.Eval('global_var') == '1'
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'step_from_func_start', os.environ['GDB_TEST_GUEST'])