# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  gdb.Command('break set_global_var')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  AssertEquals(gdb.Eval('global_var'), '2')
  AssertEquals(gdb.Eval('arg'), '1')
  gdb.ResumeAndExpectStop('finish', 'function-finished')
  AssertEquals(gdb.Eval('global_var'), '1')
  AssertEquals(gdb.Eval('local_var'), '3')
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'print_symbol')
