# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test
import os

def test(gdb):
  gdb.Command('break set_global_var')
  AssertEquals(gdb.ResumeCommand('continue')['reason'], 'breakpoint-hit')
  AssertEquals(gdb.Eval('global_var'), '2')
  AssertEquals(gdb.Eval('arg'), '1')
  AssertEquals(gdb.ResumeCommand('finish')['reason'], 'function-finished')
  AssertEquals(gdb.Eval('global_var'), '1')
  AssertEquals(gdb.Eval('local_var'), '3')
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'print_symbol', os.environ['GDB_TEST_GUEST'])
