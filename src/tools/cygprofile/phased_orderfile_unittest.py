#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for phased_orderfile.py."""

import collections
import unittest

import phased_orderfile
import process_profiles

from test_utils import (ProfileFile,
                        SimpleTestSymbol,
                        TestSymbolOffsetProcessor,
                        TestProfileManager)


class Mod10Processor(process_profiles.SymbolOffsetProcessor):
  """A restricted mock for a SymbolOffsetProcessor.

  This only implements {Translate,Get}ReacheOffsetsFromDump, and works by
  mapping a dump offset to offset - (offset % 10). If the dump offset is
  negative, it is marked as not found.
  """
  def __init__(self):
    super(Mod10Processor, self).__init__(None)

  def _TranslateReachedOffsetsFromDump(self, items, get, update):
    for i in items:
      x = get(i)
      if x >= 0:
        update(i, x - (x % 10))
      else:
        update(i, None)


class IdentityProcessor(process_profiles.SymbolOffsetProcessor):
  """A restricted mock for a SymbolOffsetProcessor.

  This only implements {Translate,Get}ReachedOffsetsFromDump, and maps the dump
  offset to itself. If the dump offset is negative, it is marked as not found.
  """
  def __init__(self):
    super(IdentityProcessor, self).__init__(None)

  def _TranslateReachedOffsetsFromDump(self, items, get, update):
    for i in items:
      x = get(i)
      if x >= 0:
        update(i, x)
      else:
        update(i, None)


class PhasedOrderfileTestCase(unittest.TestCase):

  def setUp(self):
    self._file_counter = 0

  def testGetOrderfilePhaseOffsets(self):
    mgr = TestProfileManager({
        ProfileFile(0, 0): [12, 21, -1, 33],
        ProfileFile(0, 1): [31, 49, 52],
        ProfileFile(100, 0): [113, 128],
        ProfileFile(200, 1): [132, 146],
        ProfileFile(300, 0): [19, 20, 32],
        ProfileFile(300, 1): [24, 39]})
    phaser = phased_orderfile.PhasedAnalyzer(mgr, Mod10Processor())
    opo = lambda s, c, i: phased_orderfile.OrderfilePhaseOffsets(
        startup=s, common=c, interaction=i)
    self.assertListEqual([opo([10, 20], [30], [40, 50]),
                          opo([110, 120], [], []),
                          opo([], [], [130, 140]),
                          opo([10], [20, 30], [])],
                         phaser._GetOrderfilePhaseOffsets())

  def testGetCombinedProcessOffsets(self):
    mgr = TestProfileManager({
        ProfileFile(40, 0, ''): [1, 2, 3],
        ProfileFile(50, 1, ''): [3, 4, 5],
        ProfileFile(51, 0, 'renderer'): [2, 3, 6],
        ProfileFile(51, 1, 'gpu-process'): [6, 7],
        ProfileFile(70, 0, ''): [2, 8, 9],
        ProfileFile(70, 1, ''): [9]})
    phaser = phased_orderfile.PhasedAnalyzer(mgr, IdentityProcessor())
    offsets = phaser._GetCombinedProcessOffsets('browser')
    self.assertListEqual([1, 2, 8], sorted(offsets.startup))
    self.assertListEqual([4, 5], sorted(offsets.interaction))
    self.assertListEqual([3, 9], sorted(offsets.common))

    offsets = phaser._GetCombinedProcessOffsets('gpu-process')
    self.assertListEqual([], sorted(offsets.startup))
    self.assertListEqual([6, 7], sorted(offsets.interaction))
    self.assertListEqual([], sorted(offsets.common))

    self.assertListEqual(['browser', 'gpu-process', 'renderer'],
                         sorted(phaser._GetProcessList()))

  def testGetOffsetVariations(self):
    mgr = TestProfileManager({
        ProfileFile(40, 0, ''): [1, 2, 3],
        ProfileFile(50, 1, ''): [3, 4, -10, 5],
        ProfileFile(51, 0, 'renderer'): [2, 3, 6],
        ProfileFile(51, 1, 'gpu-process'): [6, 7],
        ProfileFile(70, 0, ''): [2, 6, 8, 9],
        ProfileFile(70, 1, ''): [9]})
    phaser = phased_orderfile.PhasedAnalyzer(mgr, IdentityProcessor())
    offsets = phaser.GetOffsetsForMemoryFootprint()
    self.assertListEqual([1, 2, 8], offsets.startup)
    self.assertListEqual([6, 3, 9], offsets.common)
    self.assertListEqual([4, 5, 7], offsets.interaction)

    offsets = phaser.GetOffsetsForStartup()
    self.assertListEqual([1, 2, 6, 8], offsets.startup)
    self.assertListEqual([3, 9], offsets.common)
    self.assertListEqual([4, 5, 7], offsets.interaction)


if __name__ == "__main__":
  unittest.main()
