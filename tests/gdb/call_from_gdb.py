# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break main')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  assert gdb.Eval('test_call_from_gdb(1)') == '3'
  assert gdb.Eval('global_var') == '2'


if __name__ == '__main__':
  gdb_test.RunTest(test, 'call_from_gdb', os.environ['GDB_TEST_GUEST'])