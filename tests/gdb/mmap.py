# -*- python -*-
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test
import os


def test(gdb):
  gdb.Command('break mmap_breakpoint')
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  gdb.ResumeAndExpectStop('finish', 'function-finished')
  # Check that we can read from memory mapped files.
  AssertEquals(gdb_test.ParseNumber(gdb.Eval('*file_mapping')), 123)
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  gdb.ResumeAndExpectStop('finish', 'function-finished')
  file_mapping_str = gdb.Eval('file_mapping')
  file_mapping = gdb_test.ParseNumber(file_mapping_str)
  gdb.Command('break *' + file_mapping_str)
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  # Check that breakpoint in memory mapped code is working.
  AssertEquals(gdb.GetPC(), file_mapping)
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  gdb.ResumeAndExpectStop('finish', 'function-finished')
  file_mapping_str = gdb.Eval('file_mapping')
  file_mapping = gdb_test.ParseNumber(file_mapping_str)
  gdb.Command('break *' + file_mapping_str)
  gdb.ResumeAndExpectStop('continue', 'breakpoint-hit')
  # Check that breakpoint in memory mapped code is working.
  AssertEquals(gdb.GetPC(), file_mapping)


if __name__ == '__main__':
  os.environ['NACL_FAULT_INJECTION'] = (
      'MMAP_BYPASS_DESCRIPTOR_SAFETY_CHECK=GF/@')
  gdb_test.RunTest(test, 'mmap')