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
    # 2 hits for symbol_1, 0 for symbol_2, 1 for symbol_3
    dump = [8, 12, 48]
    reached = process_profiles.GetReachedSymbolsFromDump(
        dump, self.offset_to_symbol_info)
    self.assertListEqual([self.symbol_1, self.symbol_3], reached)
    # Ordering matters, no repetitions
    dump = [48, 12, 8, 12, 8, 16]
    reached = process_profiles.GetReachedSymbolsFromDump(
        dump, self.offset_to_symbol_info)
    self.assertListEqual([self.symbol_3, self.symbol_1], reached)

  def testSymbolNameToPrimary(self):
    symbol_infos = [SymbolInfo('1', 8, 16),
                    SymbolInfo('AnAlias', 8, 16),
                    SymbolInfo('Another', 40, 16)]
    symbol_name_to_primary = process_profiles.SymbolNameToPrimary(symbol_infos)
    self.assertDictEqual({'1': symbol_infos[0],
                          'AnAlias': symbol_infos[0],
                          'Another': symbol_infos[2]}, symbol_name_to_primary)

  def testSortedFilenames(self):
    filenames = ['second-1234-456.txt', 'first-345345-123.txt',
                 'third.bar.-789.txt']
    sorted_filenames = process_profiles._SortedFilenames(filenames)
    self.assertListEqual(
        ['first-345345-123.txt', 'second-1234-456.txt', 'third.bar.-789.txt'],
        sorted_filenames)


if __name__ == '__main__':
  unittest.main()
