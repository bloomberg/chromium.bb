# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  assert gdb.GetResultClass(gdb.SendRequest('break set_global_var')) == 'done'
  assert gdb.GetResultClass(gdb.SendRequest('continue')) == 'running'
  assert gdb.GetAsyncStatus(gdb.GetResponse()) == 'stopped'
  assert gdb.GetExpressionResult('global_var') == '2'
  assert gdb.GetExpressionResult('arg') == '1'
  assert gdb.GetResultClass(gdb.SendRequest('finish')) == 'running'
  assert gdb.GetAsyncStatus(gdb.GetResponse()) == 'stopped'
  assert gdb.GetExpressionResult('global_var') == '1'
  assert gdb.GetExpressionResult('local_var') == '3'
  gdb.SendRequest('quit')


if __name__ == '__main__':
  gdb_test.RunTest(test, 'print_symbol', os.environ['GDB_TEST_GUEST'])