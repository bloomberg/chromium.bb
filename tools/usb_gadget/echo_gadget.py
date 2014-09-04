# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""USB echo gadget module.

This gadget has pairs of IN/OUT endpoints that echo packets back to the host.
"""

import math
import struct
import uuid

import gadget
import usb_constants
import usb_descriptors


class EchoGadget(gadget.Gadget):
  """Echo gadget.
  """

  def __init__(self):
    """Create an echo gadget.
    """
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=usb_constants.VendorID.GOOGLE,
        idProduct=usb_constants.ProductID.GOOGLE_ECHO_GADGET,
        bcdUSB=0x0200,
        iManufacturer=1,
        iProduct=2,
        iSerialNumber=3,
        bcdDevice=0x0100)

    fs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)
    fs_intr_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=0,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=4,
    )
    fs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x01,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=64,
        bInterval=1  # 1ms
    ))
    fs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x81,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=64,
        bInterval=1  # 1ms
    ))
    fs_config_desc.AddInterface(fs_intr_interface_desc)

    fs_bulk_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=1,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=5
    )
    fs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x02,
        bmAttributes=usb_constants.TransferType.BULK,
        wMaxPacketSize=64,
        bInterval=0
    ))
    fs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x82,
        bmAttributes=usb_constants.TransferType.BULK,
        wMaxPacketSize=64,
        bInterval=0
    ))
    fs_config_desc.AddInterface(fs_bulk_interface_desc)

    fs_config_desc.AddInterface(usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=2,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=6
    ))
    fs_isoc_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=2,
        bAlternateSetting=1,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=6
    )
    fs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x03,
        bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
        wMaxPacketSize=1023,
        bInterval=1  # 1ms
    ))
    fs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x83,
        bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
        wMaxPacketSize=1023,
        bInterval=1  # 1ms
    ))
    fs_config_desc.AddInterface(fs_isoc_interface_desc)

    hs_config_desc = usb_descriptors.ConfigurationDescriptor(
        bmAttributes=0x80,
        MaxPower=50)

    hs_intr_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=0,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=4
    )
    hs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x01,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=64,
        bInterval=4  # 1ms
    ))
    hs_intr_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x81,
        bmAttributes=usb_constants.TransferType.INTERRUPT,
        wMaxPacketSize=64,
        bInterval=4  # 1ms
    ))
    hs_config_desc.AddInterface(hs_intr_interface_desc)

    hs_bulk_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=1,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=5
    )
    hs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x02,
        bmAttributes=usb_constants.TransferType.BULK,
        wMaxPacketSize=512,
        bInterval=0
    ))
    hs_bulk_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x82,
        bmAttributes=usb_constants.TransferType.BULK,
        wMaxPacketSize=512,
        bInterval=0
    ))
    hs_config_desc.AddInterface(hs_bulk_interface_desc)

    hs_config_desc.AddInterface(usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=2,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=6
    ))
    hs_isoc_interface_desc = usb_descriptors.InterfaceDescriptor(
        bInterfaceNumber=2,
        bAlternateSetting=1,
        bInterfaceClass=usb_constants.DeviceClass.VENDOR,
        bInterfaceSubClass=0,
        bInterfaceProtocol=0,
        iInterface=6
    )
    hs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x03,
        bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
        wMaxPacketSize=1024,
        bInterval=4  # 1ms
    ))
    hs_isoc_interface_desc.AddEndpoint(usb_descriptors.EndpointDescriptor(
        bEndpointAddress=0x83,
        bmAttributes=usb_constants.TransferType.ISOCHRONOUS,
        wMaxPacketSize=1024,
        bInterval=4  # 1ms
    ))
    hs_config_desc.AddInterface(hs_isoc_interface_desc)

    super(EchoGadget, self).__init__(
        device_desc, fs_config_desc, hs_config_desc)
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'Echo Gadget')
    self.AddStringDescriptor(3, '{:06X}'.format(uuid.getnode()))
    self.AddStringDescriptor(4, 'Interrupt Echo')
    self.AddStringDescriptor(5, 'Bulk Echo')
    self.AddStringDescriptor(6, 'Isochronous Echo')

  def ReceivePacket(self, endpoint, data):
    """Echo a packet back to the host.

    Args:
      endpoint: Incoming endpoint (must be an OUT pipe).
      data: Packet data.
    """
    assert endpoint & usb_constants.Dir.IN == 0

    self.SendPacket(endpoint | usb_constants.Dir.IN, data)

def RegisterHandlers():
  """Registers web request handlers with the application server."""

  import server
  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      server.SwitchGadget(EchoGadget())

  server.app.add_handlers('.*$', [
      (r'/echo/configure', WebConfigureHandler),
  ])
