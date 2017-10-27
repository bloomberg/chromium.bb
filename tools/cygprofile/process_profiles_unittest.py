# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for process_profiles.py."""

import collections
import unittest

import process_profiles


SymbolInfo = collections.namedtuple('SymbolInfo', ['name', 'offset', 'size'])


class ProcessProfilesTestCase(unittest.TestCase):

  def setUp(self):
    self.symbol_0 = SymbolInfo('0', 0, 0)
    self.symbol_1 = SymbolInfo('1', 8, 16)
    self.symbol_2 = SymbolInfo('2', 32, 8)
    self.symbol_3 = SymbolInfo('3', 40, 12)
    self.offset_to_symbol_info = (
        [None, None] + [self.symbol_1] * 4 + [None] * 2 + [self.symbol_2] * 2
        + [self.symbol_3] * 3)

  def testGetOffsetToSymbolInfo(self):
    symbol_infos = [self.symbol_0, self.symbol_1, self.symbol_2, self.symbol_3]
    offset_to_symbol_info = process_profiles.GetOffsetToSymbolInfo(symbol_infos)
    self.assertListEqual(self.offset_to_symbol_info, offset_to_symbol_info)

  def testGetReachedSymbolsFromDump(self):
    dump = (
        [True, False,  # No symbol
         True, True, False, False,  # 2 hits for symbol_1,
         False, False,  # Hole
         False, False,  # No symbol_2
         False, True, False  # symbol_3
        ])
    reached = process_profiles.GetReachedSymbolsFromDump(
        dump, self.offset_to_symbol_info)
    self.assertSetEqual(set([self.symbol_1, self.symbol_3]), reached)

  def testSymbolNameToPrimary(self):
    symbol_infos = [SymbolInfo('1', 8, 16),
                    SymbolInfo('AnAlias', 8, 16),
                    SymbolInfo('Another', 40, 16)]
    symbol_name_to_primary = process_profiles.SymbolNameToPrimary(symbol_infos)
    self.assertDictEqual({'1': symbol_infos[0],
                          'AnAlias': symbol_infos[0],
                          'Another': symbol_infos[2]}, symbol_name_to_primary)


if __name__ == '__main__':
  unittest.main()
