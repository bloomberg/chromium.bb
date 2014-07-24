#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

import gadget
import usb_constants
import usb_descriptors


device_desc = usb_descriptors.DeviceDescriptor(
    idVendor=0x18D1,   # Google Inc.
    idProduct=0xFF00,
    bcdUSB=0x0200,
    iManufacturer=1,
    iProduct=2,
    iSerialNumber=3,
    bNumConfigurations=1,
    bcdDevice=0x0100)

fs_config_desc = usb_descriptors.ConfigurationDescriptor(
    bmAttributes=0xC0,
    MaxPower=50)

fs_interface_desc = usb_descriptors.InterfaceDescriptor(
    bInterfaceNumber=0
)
fs_config_desc.AddInterface(fs_interface_desc)

fs_bulk_in_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x01,
    bmAttributes=usb_constants.TransferType.BULK,
    wMaxPacketSize=64,
    bInterval=0
)
fs_interface_desc.AddEndpoint(fs_bulk_in_endpoint_desc)

fs_bulk_out_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x81,
    bmAttributes=usb_constants.TransferType.BULK,
    wMaxPacketSize=64,
    bInterval=0
)
fs_interface_desc.AddEndpoint(fs_bulk_out_endpoint_desc)

fs_alt_interface_desc = usb_descriptors.InterfaceDescriptor(
    bInterfaceNumber=0,
    bAlternateSetting=1
)
fs_config_desc.AddInterface(fs_alt_interface_desc)

fs_interrupt_in_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x01,
    bmAttributes=usb_constants.TransferType.INTERRUPT,
    wMaxPacketSize=64,
    bInterval=1
)
fs_alt_interface_desc.AddEndpoint(fs_interrupt_in_endpoint_desc)

fs_interrupt_out_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x81,
    bmAttributes=usb_constants.TransferType.INTERRUPT,
    wMaxPacketSize=64,
    bInterval=1
)
fs_alt_interface_desc.AddEndpoint(fs_interrupt_out_endpoint_desc)

hs_config_desc = usb_descriptors.ConfigurationDescriptor(
    bmAttributes=0xC0,
    MaxPower=50)

hs_interface_desc = usb_descriptors.InterfaceDescriptor(
    bInterfaceNumber=0
)
hs_config_desc.AddInterface(hs_interface_desc)

hs_bulk_in_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x01,
    bmAttributes=usb_constants.TransferType.BULK,
    wMaxPacketSize=512,
    bInterval=0
)
hs_interface_desc.AddEndpoint(hs_bulk_in_endpoint_desc)

hs_bulk_out_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x81,
    bmAttributes=usb_constants.TransferType.BULK,
    wMaxPacketSize=512,
    bInterval=0
)
hs_interface_desc.AddEndpoint(hs_bulk_out_endpoint_desc)

hs_alt_interface_desc = usb_descriptors.InterfaceDescriptor(
    bInterfaceNumber=0,
    bAlternateSetting=1
)
hs_config_desc.AddInterface(hs_alt_interface_desc)

hs_interrupt_in_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x01,
    bmAttributes=usb_constants.TransferType.INTERRUPT,
    wMaxPacketSize=256,
    bInterval=1
)
hs_alt_interface_desc.AddEndpoint(hs_interrupt_in_endpoint_desc)

hs_interrupt_out_endpoint_desc = usb_descriptors.EndpointDescriptor(
    bEndpointAddress=0x81,
    bmAttributes=usb_constants.TransferType.INTERRUPT,
    wMaxPacketSize=256,
    bInterval=1
)
hs_alt_interface_desc.AddEndpoint(hs_interrupt_out_endpoint_desc)


class GadgetTest(unittest.TestCase):

  def test_get_descriptors(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    self.assertEquals(g.GetDeviceDescriptor(), device_desc)
    self.assertEquals(g.GetFullSpeedConfigurationDescriptor(), fs_config_desc)
    self.assertEquals(g.GetHighSpeedConfigurationDescriptor(), hs_config_desc)
    with self.assertRaisesRegexp(RuntimeError, 'not connected'):
      g.GetConfigurationDescriptor()

  def test_connect_full_speed(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.Connected(mock.Mock(), usb_constants.Speed.FULL)
    self.assertTrue(g.IsConnected())
    self.assertEquals(g.GetSpeed(), usb_constants.Speed.FULL)
    self.assertEquals(g.GetConfigurationDescriptor(), fs_config_desc)
    g.Disconnected()
    self.assertFalse(g.IsConnected())

  def test_connect_high_speed(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.Connected(mock.Mock(), usb_constants.Speed.HIGH)
    self.assertTrue(g.IsConnected())
    self.assertEquals(g.GetSpeed(), usb_constants.Speed.HIGH)
    self.assertEquals(g.GetConfigurationDescriptor(), hs_config_desc)
    g.Disconnected()
    self.assertFalse(g.IsConnected())

  def test_string_index_out_of_range(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    with self.assertRaisesRegexp(ValueError, 'index out of range'):
      g.AddStringDescriptor(0, 'Hello world!')

  def test_language_id_out_of_range(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    with self.assertRaisesRegexp(ValueError, 'language code out of range'):
      g.AddStringDescriptor(1, 'Hello world!', lang=-1)

  def test_get_languages(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.AddStringDescriptor(1, 'Hello world!')
    desc = g.ControlRead(0x80, 6, 0x0300, 0, 255)
    self.assertEquals(desc, '\x04\x03\x09\x04')

  def test_get_string_descriptor(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.AddStringDescriptor(1, 'Hello world!')
    desc = g.ControlRead(0x80, 6, 0x0301, 0x0409, 255)
    self.assertEquals(desc, '\x1A\x03H\0e\0l\0l\0o\0 \0w\0o\0r\0l\0d\0!\0')

  def test_get_missing_string_descriptor(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.AddStringDescriptor(1, 'Hello world!')
    desc = g.ControlRead(0x80, 6, 0x0302, 0x0409, 255)
    self.assertEquals(desc, None)

  def test_get_missing_string_language(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.AddStringDescriptor(1, 'Hello world!')
    desc = g.ControlRead(0x80, 6, 0x0301, 0x040C, 255)
    self.assertEquals(desc, None)

  def test_class_and_vendor_transfers(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    self.assertIsNone(g.ControlRead(0xA0, 0, 0, 0, 0))
    self.assertIsNone(g.ControlRead(0xC0, 0, 0, 0, 0))
    self.assertIsNone(g.ControlWrite(0x20, 0, 0, 0, ''))
    self.assertIsNone(g.ControlWrite(0x40, 0, 0, 0, ''))

  def test_set_configuration(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.ControlWrite(0, 9, 1, 0, 0)
    chip.StartEndpoint.assert_has_calls([
        mock.call(hs_bulk_in_endpoint_desc),
        mock.call(hs_bulk_out_endpoint_desc)
    ])

  def test_set_configuration_zero(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.ControlWrite(0, 9, 1, 0, 0)
    chip.StartEndpoint.reset_mock()
    g.ControlWrite(0, 9, 0, 0, 0)
    chip.StopEndpoint.assert_has_calls([
        mock.call(0x01),
        mock.call(0x81)
    ])

  def test_set_bad_configuration(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.Connected(mock.Mock(), usb_constants.Speed.HIGH)
    self.assertIsNone(g.ControlWrite(0, 9, 2, 0, 0))

  def test_set_interface(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0, 9, 1, 0, 0))
    chip.reset_mock()
    self.assertTrue(g.ControlWrite(1, 11, 1, 0, 0))
    chip.StopEndpoint.assert_has_calls([
        mock.call(0x01),
        mock.call(0x81)
    ])
    chip.StartEndpoint.assert_has_calls([
        mock.call(hs_interrupt_in_endpoint_desc),
        mock.call(hs_interrupt_out_endpoint_desc)
    ])
    chip.reset_mock()
    self.assertTrue(g.ControlWrite(1, 11, 0, 0, 0))
    chip.StopEndpoint.assert_has_calls([
        mock.call(0x01),
        mock.call(0x81)
    ])
    chip.StartEndpoint.assert_has_calls([
        mock.call(hs_bulk_in_endpoint_desc),
        mock.call(hs_bulk_out_endpoint_desc)
    ])

  def test_set_bad_interface(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    g.Connected(mock.Mock(), usb_constants.Speed.HIGH)
    self.assertTrue(g.ControlWrite(0, 9, 1, 0, 0))
    self.assertIsNone(g.ControlWrite(1, 11, 0, 1, 0))

  def test_send_packet(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.SendPacket(0x81, 'Hello world!')
    chip.SendPacket.assert_called_once_with(0x81, 'Hello world!')

  def test_send_packet_disconnected(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    with self.assertRaisesRegexp(RuntimeError, 'not connected'):
      g.SendPacket(0x81, 'Hello world!')
    g.Connected(mock.Mock(), usb_constants.Speed.HIGH)
    g.SendPacket(0x81, 'Hello world!')
    g.Disconnected()
    with self.assertRaisesRegexp(RuntimeError, 'not connected'):
      g.SendPacket(0x81, 'Hello world!')

  def test_send_invalid_endpoint(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    with self.assertRaisesRegexp(ValueError, 'non-input endpoint'):
      g.SendPacket(0x01, 'Hello world!')

  def test_receive_packet(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    self.assertIsNone(g.ReceivePacket(0x01, 'Hello world!'))

  def test_halt_endpoint(self):
    g = gadget.Gadget(device_desc, fs_config_desc, hs_config_desc)
    chip = mock.Mock()
    g.Connected(chip, usb_constants.Speed.HIGH)
    g.HaltEndpoint(0x01)
    chip.HaltEndpoint.assert_called_once_with(0x01)


if __name__ == '__main__':
  unittest.main()
