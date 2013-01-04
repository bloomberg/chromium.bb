# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  gdb.Command('break main')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  AssertEquals(gdb.Eval('test_call_from_gdb(1)'), '3')
  AssertEquals(gdb.Eval('global_var'), '2')


if __name__ == '__main__':
  gdb_test.RunTest(test, 'call_from_gdb')
