# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os

def test(gdb):
  gdb.Command('break set_global_var')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  assert gdb.Eval('global_var') == '2'
  assert gdb.Eval('arg') == '1'
  assert gdb.ResumeCommand('finish')['reason'] == 'function-finished'
  assert gdb.Eval('global_var') == '1'
  assert gdb.Eval('local_var') == '3'
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'print_symbol', os.environ['GDB_TEST_GUEST'])
