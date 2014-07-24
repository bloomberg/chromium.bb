#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import hid_constants
import hid_descriptors


class HidTest(unittest.TestCase):

  def test_keyboard_example(self):
    report_desc = hid_descriptors.ReportDescriptor(
        hid_descriptors.UsagePage(0x01),  # Generic Desktop
        hid_descriptors.Usage(0x06),  # Keyboard
        hid_descriptors.Collection(
            hid_constants.CollectionType.APPLICATION,
            hid_descriptors.UsagePage(0x07),  # Key Codes
            hid_descriptors.UsageMinimum(224),
            hid_descriptors.UsageMaximum(231),
            hid_descriptors.LogicalMinimum(0, force_length=1),
            hid_descriptors.LogicalMaximum(1),
            hid_descriptors.ReportSize(1),
            hid_descriptors.ReportCount(8),
            hid_descriptors.Input(hid_descriptors.Data,
                                  hid_descriptors.Variable,
                                  hid_descriptors.Absolute),
            hid_descriptors.ReportCount(1),
            hid_descriptors.ReportSize(8),
            hid_descriptors.Input(hid_descriptors.Constant),
            hid_descriptors.ReportCount(5),
            hid_descriptors.ReportSize(1),
            hid_descriptors.UsagePage(0x08),  # LEDs
            hid_descriptors.UsageMinimum(1),
            hid_descriptors.UsageMaximum(5),
            hid_descriptors.Output(hid_descriptors.Data,
                                   hid_descriptors.Variable,
                                   hid_descriptors.Absolute),
            hid_descriptors.ReportCount(1),
            hid_descriptors.ReportSize(3),
            hid_descriptors.Output(hid_descriptors.Constant),
            hid_descriptors.ReportCount(6),
            hid_descriptors.ReportSize(8),
            hid_descriptors.LogicalMinimum(0, force_length=1),
            hid_descriptors.LogicalMaximum(101),
            hid_descriptors.UsagePage(0x07),  # Key Codes
            hid_descriptors.UsageMinimum(0, force_length=1),
            hid_descriptors.UsageMaximum(101),
            hid_descriptors.Input(hid_descriptors.Data, hid_descriptors.Array)
        )
    )

    expected = ''.join(chr(x) for x in [
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29,
        0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05,
        0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02, 0x95, 0x01, 0x75, 0x03,
        0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05,
        0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
    ])
    self.assertEquals(report_desc, expected)

  def test_mouse_example(self):
    report_desc = hid_descriptors.ReportDescriptor(
        hid_descriptors.UsagePage(0x01),  # Generic Desktop
        hid_descriptors.Usage(0x02),  # Mouse
        hid_descriptors.Collection(
            hid_constants.CollectionType.APPLICATION,
            hid_descriptors.Usage(0x01),  # Pointer
            hid_descriptors.Collection(
                hid_constants.CollectionType.PHYSICAL,
                hid_descriptors.UsagePage(0x09),  # Buttons
                hid_descriptors.UsageMinimum(1),
                hid_descriptors.UsageMaximum(3),
                hid_descriptors.LogicalMinimum(0, force_length=1),
                hid_descriptors.LogicalMaximum(1),
                hid_descriptors.ReportCount(3),
                hid_descriptors.ReportSize(1),
                hid_descriptors.Input(hid_descriptors.Data,
                                      hid_descriptors.Variable,
                                      hid_descriptors.Absolute),
                hid_descriptors.ReportCount(1),
                hid_descriptors.ReportSize(5),
                hid_descriptors.Input(hid_descriptors.Constant),
                hid_descriptors.UsagePage(0x01),  # Generic Desktop
                hid_descriptors.Usage(0x30),  # X
                hid_descriptors.Usage(0x31),  # Y
                hid_descriptors.LogicalMinimum(0x81),  # -127
                hid_descriptors.LogicalMaximum(127),
                hid_descriptors.ReportSize(8),
                hid_descriptors.ReportCount(2),
                hid_descriptors.Input(hid_descriptors.Data,
                                      hid_descriptors.Variable,
                                      hid_descriptors.Relative)
            )
        )
    )

    expected = ''.join(chr(x) for x in [
        0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09,
        0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01,
        0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x01, 0x05, 0x01, 0x09, 0x30,
        0x09, 0x31, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
        0xC0, 0xC0
    ])
    self.assertEquals(report_desc, expected)

  def test_tag(self):
    self.assertEquals(hid_descriptors.Push(), '\xa4')

  def test_2byte_tag(self):
    self.assertEquals(hid_descriptors.LogicalMaximum(0xFF00), '\x26\x00\xFF')

  def test_4byte_tag(self):
    self.assertEquals(hid_descriptors.LogicalMaximum(0xFF884400),
                      '\x27\x00\x44\x88\xFF')

  def test_long_tag(self):
    with self.assertRaises(NotImplementedError):
      hid_descriptors.LogicalMaximum(0xFFFFFFFFFFFFFFFF)

if __name__ == '__main__':
  unittest.main()
