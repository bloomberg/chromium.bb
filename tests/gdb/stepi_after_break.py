# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  assert gdb.GetResultClass(gdb.SendRequest('break main')) == 'done'
  assert gdb.GetResultClass(gdb.SendRequest('continue')) == 'running'
  assert gdb.GetAsyncStatus(gdb.GetResponse()) != 'exit'
  assert gdb.GetResultClass(gdb.SendRequest('stepi')) == 'running'
  assert gdb.GetAsyncStatus(gdb.GetResponse()) != 'exit'
  gdb.SendRequest('quit')

  return 0

if __name__ == '__main__':
  gdb_test.RunTest(test, 'stepi_after_break', os.environ['GDB_TEST_GUEST'])