# -*- python -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gdb_test import AssertEquals
import gdb_test


def test(gdb):
  # The second superfluous call to LoadManifestFile.  This is a regression test
  # for issue https://code.google.com/p/nativeclient/issues/detail?id=3262 .
  gdb.LoadManifestFile()
  gdb.ResumeCommand('continue')
  gdb.Quit()


if __name__ == '__main__':
  gdb_test.RunTest(test, '')