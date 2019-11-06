# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.base85 import decode_base85


class TestBase85(unittest.TestCase):

    def test_decode(self):
        self.assertEqual(decode_base85('cmV?d00001'), 'x\x01\x03\x00\x00\x00\x00\x01')

    def test_decode_invalid_input(self):
        self.assertRaises(ValueError, decode_base85, '1')
        self.assertRaises(ValueError, decode_base85, '123456')
        self.assertRaises(ValueError, decode_base85, ' 2345')
        self.assertRaises(ValueError, decode_base85, '1234/')

    def test_decode_corners(self):
        self.assertEqual(decode_base85(''), '')
        self.assertEqual(decode_base85('00000'), '\x00\x00\x00\x00')
        self.assertEqual(decode_base85('|NsC0'), '\xFF\xFF\xFF\xFF')

        # acc will be larger than 0xFFFFFFFF.  Such input is invalid.
        self.assertRaises(ValueError, decode_base85, '|NsC1')
        self.assertRaises(ValueError, decode_base85, '~~~~~')
