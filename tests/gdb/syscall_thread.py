# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def CheckBacktrace(backtrace, functions):
  all_functions = [frame['frame']['func'] for frame in backtrace]
  # Check that 'functions' is a subsequence of 'all_functions'
  s1 = '|' + '|'.join(all_functions) + '|'
  s2 = '|' + '|'.join(functions) + '|'
  assert s2 in s1, '%s not in %s' % (functions, all_functions)


def test(gdb):
  gdb.Command('break inside_f3')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  # Check we stopped in inside_f3
  backtrace = gdb.Command('-stack-list-frames')
  CheckBacktrace(backtrace['stack'], ['inside_f3', 'f3'])
  # Check we have one more thread
  thread_info = gdb.Command('-thread-info')
  AssertEquals(len(thread_info['threads']), 2)
  # Select another thread
  syscall_thread_id = thread_info['threads'][0]['id']
  if syscall_thread_id == thread_info['current-thread-id']:
    syscall_thread_id = thread_info['threads'][1]['id']
  gdb.Command('-thread-select %s' % syscall_thread_id)
  # Check that thread waits in usleep
  backtrace = gdb.Command('-stack-list-frames')
  CheckBacktrace(backtrace['stack'], ['pthread_join', 'test_syscall_thread'])
  gdb.Quit()
  return 0


if __name__ == '__main__':
  gdb_test.RunTest(test, 'syscall_thread')
