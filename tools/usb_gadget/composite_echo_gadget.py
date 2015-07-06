# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import uuid

import composite_gadget
import echo_gadget
import hid_echo_gadget
import hid_gadget
import usb_constants
import usb_descriptors


class CompositeEchoGadget(composite_gadget.CompositeGadget):

  def __init__(self):
    device_desc = usb_descriptors.DeviceDescriptor(
        idVendor=usb_constants.VendorID.GOOGLE,
        idProduct=usb_constants.ProductID.GOOGLE_COMPOSITE_ECHO_GADGET,
        bcdUSB=0x0200,
        iManufacturer=1,
        iProduct=2,
        iSerialNumber=3,
        bcdDevice=0x0100)

    echo_feature = echo_gadget.EchoCompositeFeature(
        endpoints=[(0, 5, 0x81, 0x01), (1, 6, 0x82, 0x02), (2, 7, 0x83, 0x03)])

    hid_echo_feature = hid_echo_gadget.EchoFeature()
    hid_feature = hid_gadget.HidCompositeFeature(
        report_desc=hid_echo_gadget.EchoFeature.REPORT_DESC,
        features={0: hid_echo_feature},
        interface_number=3,
        interface_string=4,
        in_endpoint=0x84, out_endpoint=0x04)

    super(CompositeEchoGadget, self).__init__(
        device_desc, [echo_feature, hid_feature])
    self.AddStringDescriptor(1, 'Google Inc.')
    self.AddStringDescriptor(2, 'Echo Gadget')
    self.AddStringDescriptor(3, '{:06X}'.format(uuid.getnode()))
    self.AddStringDescriptor(4, 'HID Echo')
    self.AddStringDescriptor(5, 'Interrupt Echo')
    self.AddStringDescriptor(6, 'Bulk Echo')
    self.AddStringDescriptor(7, 'Isochronous Echo')

def RegisterHandlers():
  """Registers web request handlers with the application server."""

  import server
  from tornado import web

  class WebConfigureHandler(web.RequestHandler):

    def post(self):
      server.SwitchGadget(CompositeEchoGadget())

  server.app.add_handlers('.*$', [
      (r'/composite_echo/configure', WebConfigureHandler),
  ])
