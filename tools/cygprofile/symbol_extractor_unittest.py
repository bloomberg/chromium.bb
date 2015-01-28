#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import symbol_extractor
import unittest

class TestSymbolInfo(unittest.TestCase):
  def testIgnoresBlankLine(self):
    symbol_info = symbol_extractor.FromNmLine('')
    self.assertIsNone(symbol_info)

  def testIgnoresMalformedLine(self):
    line = ('00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev '
            'too many fields')
    symbol_info = symbol_extractor.FromNmLine(line)
    self.assertIsNone(symbol_info)
    # Wrong marker
    line = '00210d59 00000002 A _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev'
    symbol_info = symbol_extractor.FromNmLine(line)
    self.assertIsNone(symbol_info)
    # Too short
    line = '00210d59 t'
    symbol_info = symbol_extractor.FromNmLine(line)
    self.assertIsNone(symbol_info)

  def testSymbolInfoWithSize(self):
    line = '00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev'
    test_name = '_ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev'
    test_offset = 0x210d59
    test_size = 2
    symbol_info = symbol_extractor.FromNmLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(test_offset, symbol_info.offset)
    self.assertEquals(test_size, symbol_info.size)
    self.assertEquals(test_name, symbol_info.name)

  def testSymbolInfoWithoutSize(self):
    line = '0070ee8c T WebRtcSpl_ComplexBitReverse'
    test_name = 'WebRtcSpl_ComplexBitReverse'
    test_offset = 0x70ee8c
    symbol_info = symbol_extractor.FromNmLine(line)
    self.assertIsNotNone(symbol_info)
    self.assertEquals(test_offset, symbol_info.offset)
    self.assertEquals(-1, symbol_info.size)
    self.assertEquals(test_name, symbol_info.name)


class TestSymbolInfosFromStream(unittest.TestCase):
  def testSymbolInfosFromStream(self):
    lines = ['Garbage',
             '',
             ('00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev '
              'too many fields'),
             '00210d59 00000002 t _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev',
             '00210d59 00000002 A _ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev',
             '0070ee8c T WebRtcSpl_ComplexBitReverse']
    symbol_infos = symbol_extractor.SymbolInfosFromStream(lines)
    self.assertEquals(len(symbol_infos), 2)
    first = symbol_extractor.SymbolInfo(
        '_ZN34BrowserPluginHostMsg_Attach_ParamsD2Ev', 0x00210d59, 2)
    self.assertEquals(first, symbol_infos[0])
    second = symbol_extractor.SymbolInfo(
        'WebRtcSpl_ComplexBitReverse', 0x0070ee8c, -1)
    self.assertEquals(second, symbol_infos[1])


class TestSymbolInfoMappings(unittest.TestCase):
  def setUp(self):
    self.symbol_infos = [
        symbol_extractor.SymbolInfo('firstNameAtOffset', 0x42, 42),
        symbol_extractor.SymbolInfo('secondNameAtOffset', 0x42, 42),
        symbol_extractor.SymbolInfo('thirdSymbol', 0x64, 20)]

  def testGroupSymbolInfosByOffset(self):
    offset_to_symbol_info = symbol_extractor.GroupSymbolInfosByOffset(
        self.symbol_infos)
    self.assertEquals(len(offset_to_symbol_info), 2)
    self.assertIn(0x42, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x42][0], self.symbol_infos[0])
    self.assertEquals(offset_to_symbol_info[0x42][1], self.symbol_infos[1])
    self.assertIn(0x64, offset_to_symbol_info)
    self.assertEquals(offset_to_symbol_info[0x64][0], self.symbol_infos[2])

  def testCreateNameToSymbolInfos(self):
    name_to_symbol_info = symbol_extractor.CreateNameToSymbolInfo(
        self.symbol_infos)
    self.assertEquals(len(name_to_symbol_info), 3)
    for i in range(3):
      name = self.symbol_infos[i].name
      self.assertIn(name, name_to_symbol_info)
      self.assertEquals(self.symbol_infos[i], name_to_symbol_info[name])



if __name__ == '__main__':
  unittest.main()
