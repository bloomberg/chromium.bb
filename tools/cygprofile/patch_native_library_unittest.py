# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for patch_native_library.py."""

import unittest

import patch_native_library


class PatchNativeLibraryTestCase(unittest.TestCase):
  _SYMBOL_LINES = """002f7ee0 <baz>:
  2f7ee0:       b570            push    {r4, r5, r6, lr}
  2f7ee2:       4e09            ldr     r6, [pc, #36]   ; (2f7f08 <baz+0x28>)
  2f7ee4:       4674            mov     r4, lr
  2f7ee6:       4605            mov     r5, r0
  2f7ee8:       4621            mov     r1, r4
  2f7eea:       447e            add     r6, pc
  2f7eec:       4630            mov     r0, r6
  2f7eee:       f7ff ffa9       bl\t2f7e44 <__cyg_profile_func_enter>
  2f7ef2:       4806            ldr     r0, [pc, #24]   ; (2f7f0c <baz+0x2c>)
  2f7ef4:       2101            movs    r1, #1
  2f7ef6:       4478            add     r0, pc
  2f7ef8:       7001            strb    r1, [r0, #0]
  2f7efa:       4630            mov     r0, r6
  2f7efc:       4621            mov     r1, r4
  2f7efe:       f3f3 ef58       blx\t6ebdb0 <bar+0x5c>
  2f7f02:       4628            mov     r0, r5
  2f7f04:       bd70            pop     {r4, r5, r6, pc}
  2f7f06:       bf00            nop
  2f7f08:       fffffff3        .word   0xfffffff3
  2f7f0c:       101bdfef        .word   0x101bdfef
"""
  def testSymbolDataParsing(self):
    lines = self._SYMBOL_LINES.split('\n')
    symbol_data = patch_native_library.SymbolData.FromObjdumpLines(lines)
    self.assertEquals("baz", symbol_data.name)
    self.assertEquals(int("002f7ee0", 16), symbol_data.offset)
    self.assertEquals([int("2f7eee", 16)], symbol_data.bl_enter)
    self.assertEquals([], symbol_data.bl_exit)
    self.assertEquals([(int("2f7efe", 16), ("bar+0x5c"))], symbol_data.blx)

  def testFindDuplicatedInstrumentationCals(self):
    symbols_data = [
        patch_native_library.SymbolData(
            "foo", 1000, [123, 124], [125], [(126, "bar+0x12"),
                                             (127, "foo+0x12")])]
    blx_target_to_symbol_name = {"bar+0x12": "__cyg_profile_func_enter"}
    offsets = patch_native_library.FindDuplicatedInstrumentationCalls(
        symbols_data, blx_target_to_symbol_name)
    # Not the first enter call, and not the unmatched blx.
    self.assertSetEqual(set([124, 125, 126]), set(offsets))


if __name__ == '__main__':
  unittest.main()
