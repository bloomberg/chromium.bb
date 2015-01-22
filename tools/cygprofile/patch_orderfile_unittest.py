#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import patch_orderfile
import unittest


class TestPatchOrderFile(unittest.TestCase):
  def testRemoveClone(self):
    no_clone = "this.does.not.contain.clone"
    self.assertEquals(no_clone, patch_orderfile._RemoveClone(no_clone))
    with_clone = "this.does.contain.clone."
    self.assertEquals(
        "this.does.contain", patch_orderfile._RemoveClone(with_clone))

  def testGetSymbolInfosFromStreamWithSize(self):
    lines = [
        "00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev"]
    test_name = "_ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev"
    test_offset = 0x210d59
    (offset_to_symbol_infos, name_to_symbol_infos) = \
        patch_orderfile._GetSymbolInfosFromStream(lines)
    self.assertEquals(len(offset_to_symbol_infos), 1)
    self.assertEquals(len(name_to_symbol_infos), 1)
    self.assertIn(test_name, name_to_symbol_infos)
    self.assertIn(test_offset, offset_to_symbol_infos)

    self.assertEquals(len(name_to_symbol_infos[test_name]), 1)
    s = name_to_symbol_infos[test_name][0]
    self.assertEquals(test_offset, s.offset)
    self.assertEquals(2, s.size)
    self.assertEquals(test_name, s.name)

    self.assertEquals(len(offset_to_symbol_infos[test_offset]), 1)
    s = offset_to_symbol_infos[test_offset][0]
    self.assertEquals(test_offset, s.offset)
    self.assertEquals(2, s.size)
    self.assertEquals(test_name, s.name)

  def testGetSymbolInfosFromStreamWithoutSize(self):
    lines = [
        "0070ee8c T WebRtcSpl_ComplexBitReverse"]
    test_name = "WebRtcSpl_ComplexBitReverse"
    test_offset = 0x70ee8c
    (offset_to_symbol_infos, name_to_symbol_infos) = \
        patch_orderfile._GetSymbolInfosFromStream(lines)
    self.assertEquals(len(offset_to_symbol_infos), 1)
    self.assertEquals(len(name_to_symbol_infos), 1)
    self.assertIn(test_name, name_to_symbol_infos)
    self.assertIn(test_offset, offset_to_symbol_infos)

    self.assertEquals(len(name_to_symbol_infos[test_name]), 1)
    s = name_to_symbol_infos[test_name][0]
    self.assertEquals(test_offset, s.offset)
    self.assertEquals(-1, s.size)
    self.assertEquals(test_name, s.name)

    self.assertEquals(len(offset_to_symbol_infos[test_offset]), 1)
    s = offset_to_symbol_infos[test_offset][0]
    self.assertEquals(test_offset, s.offset)
    self.assertEquals(-1, s.size)
    self.assertEquals(test_name, s.name)

  def testGetSymbolsFromStream(self):
    lines = [".text.startup.",
             ".text.with.a.prefix",
             "_ZN2v88internal33HEnvironmentLivenessAnalysisPhase3RunEv"]
    names = patch_orderfile._GetSymbolsFromStream(lines)
    self.assertEquals(len(names), 2)
    self.assertEquals(
        names[0], "with.a.prefix")
    self.assertEquals(
        names[1], "_ZN2v88internal33HEnvironmentLivenessAnalysisPhase3RunEv")


  def testMatchProfiledSymbols(self):
    symbol_name = "dummySymbol"
    profiled_symbol_names = [symbol_name, "symbolThatShouldntMatch"]
    name_to_symbol_infos = {symbol_name: [
        patch_orderfile.SymbolInfo(0x42, 0x12, symbol_name)]}
    matched_symbols = patch_orderfile._MatchProfiledSymbols(
        profiled_symbol_names, name_to_symbol_infos)
    self.assertEquals(len(matched_symbols), 1)
    self.assertEquals(matched_symbols[0], name_to_symbol_infos[symbol_name][0])

  def testExpandSymbolsWithDupsFromSameOffset(self):
    symbol_name = "dummySymbol"
    symbol_name2 = "other"
    symbols = [patch_orderfile.SymbolInfo(0x42, 0x12, symbol_name)]
    offset_to_symbol_infos = {
        0x42: [patch_orderfile.SymbolInfo(0x42, 0x12, symbol_name),
               patch_orderfile.SymbolInfo(0x42, 0x12, symbol_name2)]}
    symbol_names = patch_orderfile._ExpandSymbolsWithDupsFromSameOffset(
        symbols, offset_to_symbol_infos)
    self.assertEquals(len(symbol_names), 2)
    self.assertEquals(symbol_names[0], symbol_name)
    self.assertEquals(symbol_names[1], symbol_name2)

  def testPrintSymbolWithPrefixes(self):
    class FakeOutputFile(object):
      def __init__(self):
        self.output = []
      def write(self, s):
        self.output.append(s)
    test_symbol = "dummySymbol"
    symbol_names = [test_symbol]
    fake_output = FakeOutputFile()
    patch_orderfile._PrintSymbolsWithPrefixes(symbol_names, fake_output)
    expected_output = """.text.startup.dummySymbol
.text.hot.dummySymbol
.text.unlikely.dummySymbol
.text.dummySymbol
"""
    self.assertEquals("\n".join(fake_output.output), expected_output)


if __name__ == "__main__":
  unittest.main()
