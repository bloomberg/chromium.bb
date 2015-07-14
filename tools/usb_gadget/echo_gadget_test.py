#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import echo_gadget
import usb_constants


class EchoGadgetTest(unittest.TestCase):

  def test_bulk_echo(self):
    g = echo_gadget.EchoGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)
    g.ReceivePacket(0x02, 'Hello world!')
    chip.SendPacket.assert_called_once_with(0x82, 'Hello world!')

  def test_ms_os_descriptors(self):
    g = echo_gadget.EchoGadget()
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SetConfiguration(1)

    os_string_descriptor = g.ControlRead(0x80,
                                         usb_constants.Request.GET_DESCRIPTOR,
                                         0x03EE,
                                         0x0000,
                                         0x12)
    self.assertEqual(os_string_descriptor,
                     "\x12\x03M\x00S\x00F\x00T\x001\x000\x000\x00\x01\x00")

    expected_compatid_header = \
        "\x58\x00\x00\x00\x00\x01\x04\x00\x03\0\0\0\0\0\0\0"
    compatid_header = g.ControlRead(0xC0, 0x01, 0x0000, 0x0004, 0x0010)
    self.assertEqual(compatid_header, expected_compatid_header)

    compatid_descriptor = g.ControlRead(0xC0, 0x01, 0x0000, 0x0004, 0x0058)
    self.assertEqual(compatid_descriptor,
                     expected_compatid_header +
                     "\x00\x01WINUSB\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" +
                     "\x01\x01WINUSB\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" +
                     "\x02\x01WINUSB\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")
