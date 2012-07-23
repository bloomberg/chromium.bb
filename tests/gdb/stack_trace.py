# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  gdb.Command('break leaf_call')
  assert gdb.ResumeCommand('continue')['reason'] == 'breakpoint-hit'
  result = gdb.Command('-stack-list-frames 0 2')
  assert result['stack'][0]['frame']['func'] == 'leaf_call'
  assert result['stack'][1]['frame']['func'] == 'nested_calls'
  assert result['stack'][2]['frame']['func'] == 'main'

  result = gdb.Command('-stack-list-arguments 1 0 1')
  assert result['stack-args'][0]['frame']['args'][0]['value'] == '2'
  assert result['stack-args'][1]['frame']['args'][0]['value'] == '1'
  gdb.Command('return')
  assert gdb.ResumeCommand('finish')['reason'] == 'function-finished'
  assert gdb.Eval('global_var') == '1'


if __name__ == '__main__':
  gdb_test.RunTest(test, 'stack_trace', os.environ['GDB_TEST_GUEST'])