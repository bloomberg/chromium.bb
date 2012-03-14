#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verify that crx_id.py generates a reasonable string for a canned CRX file.
"""

import crx_id
import os
import shutil
import sys
import unittest
import tempfile

CRX_ID_DIR = os.path.dirname(sys.argv[0])

class CrxIdUnittest(unittest.TestCase):

  PACKED_CRX = os.path.join(CRX_ID_DIR,
                            'jebgalgnebhfojomionfpkfelancnnkf.crx')

  PACKED_APP_ID = 'jebgalgnebhfojomionfpkfelancnnkf'
  PACKED_HASH_BYTES = \
      '{0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,' \
      ' 0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,' \
      ' 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,' \
      ' 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40}'

  def testPackedHashAppId(self):
    """ Test the output generated for a canned, packed CRX. """
    self.assertEqual(crx_id.GetCRXAppID(self.PACKED_CRX),
                     self.PACKED_APP_ID)
    self.assertEqual(crx_id.GetCRXHash(self.PACKED_CRX),
                     self.PACKED_HASH_BYTES)


  # ../../chrome/test/data/extensions/unpacked/manifest_with_key.json
  BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(sys.argv[0])))
  UNPACKED_TEST_DIR = os.path.join(
      BASE_DIR,
      'chrome', 'test', 'data', 'extensions', 'unpacked')
  UNPACKED_APP_ID = 'cbcdidchbppangcjoddlpdjlenngjldk'
  UNPACKED_HASH_BYTES = \
      '{0x21, 0x23, 0x83, 0x27, 0x1f, 0xf0, 0xd6, 0x29,' \
      ' 0xe3, 0x3b, 0xf3, 0x9b, 0x4d, 0xd6, 0x9b, 0x3a,' \
      ' 0xff, 0x7d, 0x6b, 0xc4, 0x78, 0x30, 0x47, 0xa6,' \
      ' 0x23, 0x12, 0x72, 0x84, 0x9b, 0x9a, 0xf6, 0x3c}'

  def testUnpackedHashAppId(self):
    """ Test the output generated for a canned, unpacked extension. """
    temp_unpacked_crx = tempfile.mkdtemp()
    shutil.copy2(os.path.join(self.UNPACKED_TEST_DIR,
                              'manifest_with_key.json'),
                 os.path.join(temp_unpacked_crx,
                              'manifest.json'))
    self.assertEqual(crx_id.GetCRXAppID(temp_unpacked_crx),
                     self.UNPACKED_APP_ID)
    self.assertEqual(crx_id.GetCRXHash(temp_unpacked_crx),
                     self.UNPACKED_HASH_BYTES)
    # This uses the path to compute the AppID.
    self.assertEqual(crx_id.GetCRXAppID('/tmp/temp_extension',
                                        from_test_path=True),
                     'ajbbicncdkdlchpjplgjaglppbcbmaji')
    # Test drive letter normalization.
    kWinPathId = 'popnagglbbhjlobnnbcjnckakjoegnjp'
    self.assertEqual(crx_id.GetCRXAppID('c:\temp_extension',
                                        from_test_path=True),
                     kWinPathId)
    self.assertEqual(crx_id.GetCRXAppID('C:\temp_extension',
                                        from_test_path=True),
                     kWinPathId)
    shutil.rmtree(temp_unpacked_crx)


if __name__ == '__main__':
  unittest.main()
