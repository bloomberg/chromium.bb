#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import patch_orderfile
import symbol_extractor


class TestPatchOrderFile(unittest.TestCase):
  def testRemoveClone(self):
    no_clone = "this.does.not.contain.clone"
    self.assertEquals(no_clone, patch_orderfile._RemoveClone(no_clone))
    with_clone = "this.does.contain.clone."
    self.assertEquals(
        "this.does.contain", patch_orderfile._RemoveClone(with_clone))

  def testAliasClonedSymbols(self):
    symbol_infos = [
        symbol_extractor.SymbolInfo(name='aSymbol', offset=0x42, size=0x12,
                                    section='.text'),
        symbol_extractor.SymbolInfo(name='aSymbol.clone.', offset=8, size=1,
                                    section='.text')]
    (offset_to_symbol_infos, name_to_symbol_infos) = \
        patch_orderfile._GroupSymbolInfos(symbol_infos)
    self.assertEquals(len(offset_to_symbol_infos), 2)
    for i in range(2):
      s = symbol_infos[i]
      matching = offset_to_symbol_infos[s.offset][0]
      self.assertEquals(matching.offset, s.offset)
      self.assertEquals(matching.size, s.size)
    self.assertEquals(len(name_to_symbol_infos), 1)
    self.assertEquals(len(name_to_symbol_infos['aSymbol']), 2)

  def testGroupSymbolsByOffset(self):
    symbol_infos = (
        symbol_extractor.SymbolInfo(name='aSymbol', offset=0x42, size=0x12,
                                    section='.text'),
        symbol_extractor.SymbolInfo(name='anotherSymbol', offset=0x42, size=1,
                                    section='.text'))
    (offset_to_symbol_infos, _) = \
        patch_orderfile._GroupSymbolInfos(symbol_infos)
    self.assertEquals(len(offset_to_symbol_infos), 1)
    self.assertEquals(tuple(offset_to_symbol_infos[0x42]), symbol_infos)

  def testExpandSymbols(self):
    symbol_name = "dummySymbol"
    symbol_name2 = "other"
    profiled_symbol_names = [symbol_name, "symbolThatShouldntMatch"]
    name_to_symbol_infos = {symbol_name: [
        symbol_extractor.SymbolInfo(symbol_name, 0x42, 0x12,
                                    section='.text')]}
    offset_to_symbol_infos = {
        0x42: [symbol_extractor.SymbolInfo(symbol_name, 0x42, 0x12,
                                           section='.text'),
               symbol_extractor.SymbolInfo(symbol_name2, 0x42, 0x12,
                                           section='.text')]}
    symbol_names = patch_orderfile._ExpandSymbols(
        profiled_symbol_names, name_to_symbol_infos, offset_to_symbol_infos)
    self.assertEquals(len(symbol_names), 3)
    self.assertEquals(symbol_names[0], symbol_name)
    self.assertEquals(symbol_names[1], symbol_name2)
    self.assertEquals(symbol_names[2], "symbolThatShouldntMatch")

  def testPrintSymbolWithPrefixes(self):
    class FakeOutputFile(object):
      def __init__(self):
        self.output = ''
      def write(self, s):
        self.output = self.output + s
    test_symbol = "dummySymbol"
    symbol_names = [test_symbol]
    fake_output = FakeOutputFile()
    patch_orderfile._PrintSymbolsWithPrefixes(symbol_names, fake_output)
    expected_output = """.text.startup.dummySymbol
.text.hot.dummySymbol
.text.unlikely.dummySymbol
.text.dummySymbol
"""
    self.assertEquals(fake_output.output, expected_output)


if __name__ == "__main__":
  unittest.main()
