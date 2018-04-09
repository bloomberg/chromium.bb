#!/usr/bin/python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import code_pages_pss


class CodePagePssTestCase(unittest.TestCase):
  def testNoMatchingEntry(self):
    CONTENT = """7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0      [vdso]
Size:                  8 kB
Rss:                   4 kB
Pss:                   0 kB
Shared_Clean:          4 kB
Shared_Dirty:          0 kB
Private_Clean:         0 kB
Private_Dirty:         0 kB
Referenced:            4 kB
Anonymous:             0 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Locked:                0 kB
VmFlags: rd ex mr mw me de sd
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0      [vsyscall]
Size:                  4 kB
Rss:                   0 kB
Pss:                   0 kB
Shared_Clean:          0 kB
Shared_Dirty:          0 kB
Private_Clean:         0 kB
Private_Dirty:         0 kB
Referenced:            0 kB
Anonymous:             0 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
KernelPageSize:        4 kB
MMUPageSize:           4 kB
Locked:                0 kB
VmFlags: rd ex""".split('\n')
    self.assertEquals(0, code_pages_pss._GetPssFromProcSmapsLinesInKb(
        CONTENT, 'com.android.chrome', False))

  def testMatchingEntry(self):
    CONTENT = """7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0 foo.apk
Rss:                    0 kB
Pss:                   12 kB
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0      [vsyscall]
Rss:                   0 kB
Pss:                   0 kB
VmFlags: rd ex""".split('\n')
    self.assertEquals(12, code_pages_pss._GetPssFromProcSmapsLinesInKb(
        CONTENT, 'foo', False))

  def testMatchingEntries(self):
    CONTENT = """7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0 foo.so
Rss:                    55 kB
Pss:                   12 kB
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0 foo
Rss:                   14 kB
Pss:                   22 kB
VmFlags: rd ex""".split('\n')
    self.assertEquals(34, code_pages_pss._GetPssFromProcSmapsLinesInKb(
        CONTENT, 'foo', False))

  def testMalformedFile(self):
    CONTENT = """Not the right line
7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0 foo
Rss:                    55 kB
Pss:                   12 kB
""".split('\n')
    with self.assertRaises(AssertionError):
      code_pages_pss._GetPssFromProcSmapsLinesInKb(CONTENT, 'foo', False)
    CONTENT = """7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0 foo
Rss:                    55 kB
Pss:                   12 kB
7fff4fbfc000-7fff4fbfe000 r-xp 00000000 00:00 0 foo
Rss:                    55 kB
Unexpected line
Pss:                   12 kB
""".split('\n')
    with self.assertRaises(AssertionError):
      code_pages_pss._GetPssFromProcSmapsLinesInKb(CONTENT, 'foo', False)


if __name__ == '__main__':
  unittest.main()
