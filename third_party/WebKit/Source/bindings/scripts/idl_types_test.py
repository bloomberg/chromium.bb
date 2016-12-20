# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Unit tests for idl_types.py."""

import unittest

from idl_types import IdlType


class IdlTypeTest(unittest.TestCase):

    def test_is_void(self):
        idl_type = IdlType('void')
        self.assertTrue(idl_type.is_void)
        idl_type = IdlType('somethingElse')
        self.assertFalse(idl_type.is_void)
