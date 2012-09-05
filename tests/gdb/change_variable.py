# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break test_change_variable')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  gdb.Command('set var arg = 2')
  assert gdb.Eval('arg') == '2'
  gdb.ResumeCommand('step')
  assert gdb.Eval('local_var') == '4'
  gdb.Command('set var local_var = 5')
  assert gdb.Eval('local_var') == '5'
  gdb.Command('set var global_var = 1')
  assert gdb.Eval('global_var') == '1'
  gdb.ResumeCommand('step')
  assert gdb.Eval('global_var') == '8' # 1 + 5 + 2


if __name__ == '__main__':
  gdb_test.RunTest(test, 'change_variable', os.environ['GDB_TEST_GUEST'])