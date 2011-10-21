#!/usr/bin/env python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verify that crx_id.py generates a reasonable string for a canned CRX file.
"""

import crx_id
import os
import sys
import unittest

CANNED_CRX = os.path.join(os.path.dirname(sys.argv[0]),
                           'jebgalgnebhfojomionfpkfelancnnkf.crx')
CRX_ID_SCRIPT = os.path.join(os.path.dirname(sys.argv[0]),
                           'crx_id.py')
EXPECTED_HASH_BYTES = \
    '{0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,' \
    ' 0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,' \
    ' 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,' \
    ' 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40}'

EXPECTED_APP_ID = 'jebgalgnebhfojomionfpkfelancnnkf'

class CrxIdUnittest(unittest.TestCase):
  def testHashAppId(self):
    """ Test that the output generated for a canned CRX. """
    self.assertEqual(crx_id.GetCRXAppID(CANNED_CRX),
                     EXPECTED_APP_ID)
    self.assertEqual(crx_id.GetCRXHash(CANNED_CRX),
                     EXPECTED_HASH_BYTES)

if __name__ == '__main__':
  unittest.main()
