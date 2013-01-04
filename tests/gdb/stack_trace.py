# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  gdb.Command('break leaf_call')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  result = gdb.Command('-stack-list-frames 0 2')
  AssertEquals(result['stack'][0]['frame']['func'], 'leaf_call')
  AssertEquals(result['stack'][1]['frame']['func'], 'nested_calls')
  AssertEquals(result['stack'][2]['frame']['func'], 'main')

  result = gdb.Command('-stack-list-arguments 1 0 1')
  AssertEquals(result['stack-args'][0]['frame']['args'][0]['value'], '2')
  AssertEquals(result['stack-args'][1]['frame']['args'][0]['value'], '1')
  gdb.Command('return')
  gdb.ResumeAndExpectStop('finish', 'function-finished')
  AssertEquals(gdb.Eval('global_var'), '1')


if __name__ == '__main__':
  gdb_test.RunTest(test, 'stack_trace')
