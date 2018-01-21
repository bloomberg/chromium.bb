# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for process_profiles.py."""

import collections
import unittest

import process_profiles


SymbolInfo = collections.namedtuple('SymbolInfo', ['name', 'offset', 'size'])


class TestSymbolOffsetProcessor(process_profiles.SymbolOffsetProcessor):
  def __init__(self, symbol_infos):
    super(TestSymbolOffsetProcessor, self).__init__(None)
    self._symbol_infos = symbol_infos


class ProcessProfilesTestCase(unittest.TestCase):

  def setUp(self):
    self.symbol_0 = SymbolInfo('0', 0, 0)
    self.symbol_1 = SymbolInfo('1', 8, 16)
    self.symbol_2 = SymbolInfo('2', 32, 8)
    self.symbol_3 = SymbolInfo('3', 40, 12)
    self.offset_to_symbol_info = (
        [None, None] + [self.symbol_1] * 4 + [None] * 2 + [self.symbol_2] * 2
        + [self.symbol_3] * 3)
    self.symbol_infos = [self.symbol_0, self.symbol_1,
                         self.symbol_2, self.symbol_3]

  def testGetOffsetToSymbolInfo(self):
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    offset_to_symbol_info = processor._GetDumpOffsetToSymbolInfo()
    self.assertListEqual(self.offset_to_symbol_info, offset_to_symbol_info)

  def testGetReachedSymbolsFromDump(self):
    processor = TestSymbolOffsetProcessor(self.symbol_infos)
    # 2 hits for symbol_1, 0 for symbol_2, 1 for symbol_3
    dump = [8, 12, 48]
    reached = processor.GetReachedOffsetsFromDump(dump)
    self.assertListEqual([self.symbol_1.offset, self.symbol_3.offset], reached)
    # Ordering matters, no repetitions
    dump = [48, 12, 8, 12, 8, 16]
    reached = processor.GetReachedOffsetsFromDump(dump)
    self.assertListEqual([self.symbol_3.offset, self.symbol_1.offset], reached)

  def testSymbolNameToPrimary(self):
    symbol_infos = [SymbolInfo('1', 8, 16),
                    SymbolInfo('AnAlias', 8, 16),
                    SymbolInfo('Another', 40, 16)]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertDictEqual({8: symbol_infos[0],
                          40: symbol_infos[2]}, processor.OffsetToPrimaryMap())

  def testPrimarySizeMismatch(self):
    symbol_infos = [SymbolInfo('1', 8, 16),
                    SymbolInfo('AnAlias', 8, 32)]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertRaises(AssertionError, processor.OffsetToPrimaryMap)
    symbol_infos = [SymbolInfo('1', 8, 0),
                    SymbolInfo('2', 8, 32),
                    SymbolInfo('3', 8, 32),
                    SymbolInfo('4', 8, 0),]
    processor = TestSymbolOffsetProcessor(symbol_infos)
    self.assertDictEqual({8: symbol_infos[1]}, processor.OffsetToPrimaryMap())


  def testMatchSymbols(self):
    symbols_b = [SymbolInfo('W', 30, 10),
                 SymbolInfo('Y', 60, 5),
                 SymbolInfo('X', 100, 10)]
    processor_b = TestSymbolOffsetProcessor(symbols_b)
    self.assertListEqual(symbols_b[1:3],
                         processor_b.MatchSymbolNames(['Y', 'X']))

  def testSortedFilenames(self):
    filenames = ['second-1234-456.txt', 'first-345345-123.txt',
                 'third.bar.-789.txt']
    sorted_filenames = process_profiles._SortedFilenames(filenames)
    self.assertListEqual(
        ['first-345345-123.txt', 'second-1234-456.txt', 'third.bar.-789.txt'],
        sorted_filenames)


if __name__ == '__main__':
  unittest.main()
