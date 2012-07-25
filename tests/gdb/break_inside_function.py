# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gdb_test
import os


def test(gdb):
  # Get the line number of the function by setting and removing breakpoint.
  # -symbol-info-line designed for this purpose is removed from modern
  # versions of gdb. Other ways include parsing of gdb output from the
  # "info line" command and searching through result of the list command
  # which are less beautiful.
  # Since we don't have symbols at the start of the program when testing
  # with glibc, program symbols are loaded explicitly.
  gdb.LoadSymbolFile()
  result = gdb.Command('-break-insert test_two_line_function')
  line = int(result['bkpt']['line'])
  gdb.Command('-break-delete ' + result['bkpt']['number'])
  gdb.ReloadSymbols()

  gdb.Command('break gdb_test_guest.c:' + str(line + 1))
  result = gdb.ResumeCommand('continue')
  assert result['reason'] == 'breakpoint-hit'
  assert result['frame']['line'] == str(line + 1)
  gdb.Command('break gdb_test_guest.c:' + str(line + 2))
  result = gdb.ResumeCommand('continue')
  assert result['reason'] == 'breakpoint-hit'
  assert result['frame']['line'] == str(line + 2)
  # If program is successfully finished, the dictionary will be empty
  assert gdb.ResumeCommand('continue') == {}
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, 'break_inside_function', os.environ['GDB_TEST_GUEST'])
