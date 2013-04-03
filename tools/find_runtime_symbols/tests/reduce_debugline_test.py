#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import logging
import os
import sys
import textwrap
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import reduce_debugline


class ReduceDebuglineTest(unittest.TestCase):
  _DECODED_DEBUGLINE = textwrap.dedent("""\
      Decoded dump of debug contents of section .debug_line:

      CU: ../../chrome/app/chrome_exe_main_gtk.cc:
      File name                            Line number    Starting address
      chrome_exe_main_gtk.cc                        33            0xa3be50

      chrome_exe_main_gtk.cc                        34            0xa3be66
      chrome_exe_main_gtk.cc                        39            0xa3be75
      chrome_exe_main_gtk.cc                        42            0xa3be7a

      CU: ../../chrome/app/chrome_main.cc:
      File name                            Line number    Starting address
      chrome_main.cc                                30            0xa3be90

      chrome_main.cc                                31            0xa3bea3
      chrome_main.cc                                32            0xa3beaf
      chrome_main.cc                                34            0xa3bec9
      chrome_main.cc                                32            0xa3bed1

      CU: ../../chrome/app/chrome_main_delegate.cc:
      File name                            Line number    Starting address
      chrome_main_delegate.cc                      320            0xa3bee0

      chrome_main_delegate.cc                      320            0xa3bef0
      chrome_main_delegate.cc                      321            0xa3bf43
      chrome_main_delegate.cc                      322            0xa3bf48
      chrome_main_delegate.cc                      324            0xa3bf50
      chrome_main_delegate.cc                      324            0xa3bf60

      chrome_main_delegate.cc                      612            0xa3cd54
      chrome_main_delegate.cc                      617            0xa3cd6b
      chrome_main_delegate.cc                      299            0xa3d5fd
      chrome_main_delegate.cc                      300            0xa3d605

      ../../content/public/app/content_main_delegate.h:
      content_main_delegate.h                       22            0xa3d620

      content_main_delegate.h                       22            0xa3d637

      ../../chrome/common/chrome_content_client.h:
      chrome_content_client.h                       16            0xa3d640

      chrome_content_client.h                       16            0xa3d650

      ../../base/memory/scoped_ptr.h:
      scoped_ptr.h                                 323            0xa3d680

      scoped_ptr.h                                 323            0xa3d690

      ../../base/memory/scoped_ptr.h:
      scoped_ptr.h                                 323            0xa3d660

      scoped_ptr.h                                 323            0xa3d670

      ../../base/memory/scoped_ptr.h:
      scoped_ptr.h                                 428            0xa3d6a0

      scoped_ptr.h                                 428            0xa3d6b0

      CU: ../../something.c:
      File name                            Line number    Starting address
      something.c                                   57           0x76e2cc0

      something.c                                   62           0x76e2cd3
      something.c                                   64           0x76e2cda
      something.c                                   65           0x76e2ce9
      something.c                                   66           0x76e2cf8

      """)

  _EXPECTED_REDUCED_DEBUGLINE = {
      '../../chrome/app/chrome_exe_main_gtk.cc': [
          (0xa3be50, 0xa3be50),
          (0xa3be66, 0xa3be7a),
          ],
      '../../chrome/app/chrome_main.cc': [
          (0xa3be90, 0xa3be90),
          (0xa3bea3, 0xa3bed1),
          ],
      '../../chrome/app/chrome_main_delegate.cc': [
          (0xa3bee0, 0xa3bee0),
          (0xa3bef0, 0xa3bf60),
          (0xa3cd54, 0xa3d605),
          ],
      '../../content/public/app/content_main_delegate.h': [
          (0xa3d620, 0xa3d620),
          (0xa3d637, 0xa3d637),
          ],
      '../../chrome/common/chrome_content_client.h': [
          (0xa3d640, 0xa3d640),
          (0xa3d650, 0xa3d650),
          ],
      '../../base/memory/scoped_ptr.h': [
          (0xa3d680, 0xa3d680),
          (0xa3d690, 0xa3d690),
          (0xa3d660, 0xa3d660),
          (0xa3d670, 0xa3d670),
          (0xa3d6a0, 0xa3d6a0),
          (0xa3d6b0, 0xa3d6b0),
          ],
      '../../something.c': [
          (0x76e2cc0, 0x76e2cc0),
          (0x76e2cd3, 0x76e2cf8),
          ],
      }

  def test(self):
    ranges_dict = reduce_debugline.reduce_decoded_debugline(
        cStringIO.StringIO(self._DECODED_DEBUGLINE))
    self.assertEqual(self._EXPECTED_REDUCED_DEBUGLINE, ranges_dict)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
