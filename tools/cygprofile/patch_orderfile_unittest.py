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

  def testSymbolsWithSameOffset(self):
    symbol_name = "dummySymbol"
    symbol_name2 = "other"
    name_to_symbol_infos = {symbol_name: [
        symbol_extractor.SymbolInfo(symbol_name, 0x42, 0x12,
                                    section='.text')]}
    offset_to_symbol_infos = {
        0x42: [symbol_extractor.SymbolInfo(symbol_name, 0x42, 0x12,
                                           section='.text'),
               symbol_extractor.SymbolInfo(symbol_name2, 0x42, 0x12,
                                           section='.text')]}
    symbol_names = patch_orderfile._SymbolsWithSameOffset(
        symbol_name, name_to_symbol_infos, offset_to_symbol_infos)
    self.assertEquals(len(symbol_names), 2)
    self.assertEquals(symbol_names[0], symbol_name)
    self.assertEquals(symbol_names[1], symbol_name2)
    self.assertEquals([], patch_orderfile._SymbolsWithSameOffset(
        "symbolThatShouldntMatch",
        name_to_symbol_infos, offset_to_symbol_infos))

  def testExpandSection(self):
    symbol_name1 = 'symbol1'
    symbol_name2 = 'symbol2'
    symbol_name3 = 'symbol3'
    section_name1 = '.text.' + symbol_name1
    section_name3 = '.text.foo'
    name_to_symbol_infos = {symbol_name1: [
        symbol_extractor.SymbolInfo(symbol_name1, 0x42, 0x12,
                                    section='.text')]}
    offset_to_symbol_infos = {
        0x42: [symbol_extractor.SymbolInfo(symbol_name1, 0x42, 0x12,
                                           section='.text'),
               symbol_extractor.SymbolInfo(symbol_name2, 0x42, 0x12,
                                           section='.text')]}
    section_to_symbols_map = {section_name1: [symbol_name1],
                              section_name3: [symbol_name1, symbol_name3]}
    symbol_to_sections_map = {symbol_name1:
                                  [section_name1, section_name3],
                              symbol_name3: [section_name3]}
    expanded_sections = list(patch_orderfile._ExpandSection(
                               section_name1,
                               name_to_symbol_infos, offset_to_symbol_infos,
                               section_to_symbols_map, symbol_to_sections_map))
    self.assertEqual(6, len(expanded_sections))
    self.assertEqual(section_name1, expanded_sections[0])
    self.assertIn(section_name3, expanded_sections)
    self.assertIn('.text.startup.symbol2', expanded_sections)
    self.assertIn('.text.hot.symbol2', expanded_sections)
    self.assertIn('.text.unlikely.symbol2', expanded_sections)
    self.assertIn('.text.symbol2', expanded_sections)


if __name__ == "__main__":
  unittest.main()
