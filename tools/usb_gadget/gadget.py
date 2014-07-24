# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic USB gadget functionality.
"""

import struct

import usb_constants


class Gadget(object):
  """Basic functionality for a USB device.

  Implements standard control requests assuming that a subclass will handle
  class- or vendor-specific requests.
  """

  def __init__(self, device_desc, fs_config_desc, hs_config_desc):
    """Create a USB gadget device.

    Args:
      device_desc: USB device descriptor.
      fs_config_desc: Low/full-speed device descriptor.
      hs_config_desc: High-speed device descriptor.
    """
    self._speed = usb_constants.Speed.UNKNOWN
    self._chip = None
    self._device_desc = device_desc
    self._fs_config_desc = fs_config_desc
    self._hs_config_desc = hs_config_desc
    # dict mapping language codes to a dict mapping indexes to strings
    self._strings = {}
    # dict mapping interface numbers to a set of endpoint addresses
    self._active_endpoints = {}

  def GetDeviceDescriptor(self):
    return self._device_desc

  def GetFullSpeedConfigurationDescriptor(self):
    return self._fs_config_desc

  def GetHighSpeedConfigurationDescriptor(self):
    return self._hs_config_desc

  def GetConfigurationDescriptor(self):
    if self._speed == usb_constants.Speed.FULL:
      return self._fs_config_desc
    elif self._speed == usb_constants.Speed.HIGH:
      return self._hs_config_desc
    else:
      raise RuntimeError('Device is not connected.')

  def GetSpeed(self):
    return self._speed

  def AddStringDescriptor(self, index, value, lang=0x0409):
    """Add a string descriptor to this device.

    Args:
      index: String descriptor index (matches 'i' fields in descriptors).
      value: The string.
      lang: Language code (default: English).

    Raises:
      ValueError: The index or language code is invalid.
    """
    if index < 1 or index > 255:
      raise ValueError('String descriptor index out of range.')
    if lang < 0 or lang > 0xffff:
      raise ValueError('String descriptor language code out of range.')

    lang_strings = self._strings.setdefault(lang, {})
    lang_strings[index] = value

  def Connected(self, chip, speed):
    """The device has been connected to a USB host.

    Args:
      chip: USB controller.
      speed: Connection speed.
    """
    self._speed = speed
    self._chip = chip

  def Disconnected(self):
    """The device has been disconnected from the USB host."""
    self._speed = usb_constants.Speed.UNKNOWN
    self._chip = None
    self._active_endpoints.clear()

  def IsConnected(self):
    return self._chip is not None

  def ControlRead(self, request_type, request, value, index, length):
    """Handle a read on the control pipe (endpoint zero).

    Args:
      request_type: bmRequestType field of the setup packet.
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    assert request_type & usb_constants.Dir.IN
    typ = request_type & usb_constants.Type.MASK
    recipient = request_type & usb_constants.Recipient.MASK
    if typ == usb_constants.Type.STANDARD:
      return self.StandardControlRead(
          recipient, request, value, index, length)
    elif typ == usb_constants.Type.CLASS:
      return self.ClassControlRead(
          recipient, request, value, index, length)
    elif typ == usb_constants.Type.VENDOR:
      return self.VendorControlRead(
          recipient, request, value, index, length)

  def ControlWrite(self, request_type, request, value, index, data):
    """Handle a write to the control pipe (endpoint zero).

    Args:
      request_type: bmRequestType field of the setup packet.
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    assert not request_type & usb_constants.Dir.IN
    typ = request_type & usb_constants.Type.MASK
    recipient = request_type & usb_constants.Recipient.MASK
    if typ == usb_constants.Type.STANDARD:
      return self.StandardControlWrite(
          recipient, request, value, index, data)
    elif typ == usb_constants.Type.CLASS:
      return self.ClassControlWrite(
          recipient, request, value, index, data)
    elif typ == usb_constants.Type.VENDOR:
      return self.VendorControlWrite(
          recipient, request, value, index, data)

  def SendPacket(self, endpoint, data):
    """Send a data packet on the given endpoint.

    Args:
      endpoint: Endpoint address.
      data: Data buffer.

    Raises:
      ValueError: If the endpoint address is not valid.
      RuntimeError: If the device is not connected.
    """
    if self._chip is None:
      raise RuntimeError('Device is not connected.')
    if not endpoint & usb_constants.Dir.IN:
      raise ValueError('Cannot write to non-input endpoint.')
    self._chip.SendPacket(endpoint, data)

  def ReceivePacket(self, endpoint, data):
    """Handle an incoming data packet on one of the device's active endpoints.

    This method should be overridden by a subclass implementing endpoint-based
    data transfers.

    Args:
      endpoint: Endpoint address.
      data: Data buffer.
    """
    pass

  def HaltEndpoint(self, endpoint):
    """Signals a STALL condition to the host on the given endpoint.

    Args:
      endpoint: Endpoint address.
    """
    self._chip.HaltEndpoint(endpoint)

  def StandardControlRead(self, recipient, request, value, index, length):
    """Handle standard control transfers.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    if request == usb_constants.Request.GET_DESCRIPTOR:
      desc_type = value >> 8
      desc_index = value & 0xff
      desc_lang = index

      print 'GetDescriptor(recipient={}, type={}, index={}, lang={})'.format(
          recipient, desc_type, desc_index, desc_lang)

      return self.GetDescriptor(recipient, desc_type, desc_index, desc_lang,
                                length)

  def GetDescriptor(self, recipient, typ, index, lang, length):
    """Handle a standard GET_DESCRIPTOR request.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.3.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      typ: Descriptor type.
      index: Descriptor index.
      lang: Descriptor language code.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The value of the descriptor or None to stall the pipe.
    """
    if recipient == usb_constants.Recipient.DEVICE:
      if typ == usb_constants.DescriptorType.STRING:
        return self.GetStringDescriptor(index, lang, length)

  def ClassControlRead(self, recipient, request, value, index, length):
    """Handle class-specific control transfers.

    This function should be overridden by a subclass implementing a particular
    device class.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    _ = recipient, request, value, index, length
    return None

  def VendorControlRead(self, recipient, request, value, index, length):
    """Handle vendor-specific control transfers.

    This function should be overridden by a subclass if implementing a device
    that responds to vendor-specific requests.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      A buffer to return to the USB host with len <= length on success or
      None to stall the pipe.
    """
    _ = recipient, request, value, index, length
    return None

  def StandardControlWrite(self, recipient, request, value, index, data):
    """Handle standard control transfers.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = data

    if request == usb_constants.Request.SET_CONFIGURATION:
      if recipient == usb_constants.Recipient.DEVICE:
        return self.SetConfiguration(value)
    elif request == usb_constants.Request.SET_INTERFACE:
      if recipient == usb_constants.Recipient.INTERFACE:
        return self.SetInterface(index, value)

  def ClassControlWrite(self, recipient, request, value, index, data):
    """Handle class-specific control transfers.

    This function should be overridden by a subclass implementing a particular
    device class.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None

  def VendorControlWrite(self, recipient, request, value, index, data):
    """Handle vendor-specific control transfers.

    This function should be overridden by a subclass if implementing a device
    that responds to vendor-specific requests.

    Args:
      recipient: Request recipient (device, interface, endpoint, etc.)
      request: bRequest field of the setup packet.
      value: wValue field of the setup packet.
      index: wIndex field of the setup packet.
      data: Data stage of the request.

    Returns:
      True on success, None to stall the pipe.
    """
    _ = recipient, request, value, index, data
    return None

  def GetStringDescriptor(self, index, lang, length):
    """Handle a GET_DESCRIPTOR(String) request from the host.

    Descriptor index 0 returns the set of languages supported by the device.
    All other indices return the string descriptors registered with those
    indices.

    See Universal Serial Bus Specification Revision 2.0 section 9.6.7.

    Args:
      index: Descriptor index.
      lang: Descriptor language code.
      length: Maximum amount of data the host expects the device to return.

    Returns:
      The string descriptor or None to stall the pipe if the descriptor is not
      found.
    """
    if index == 0:
      length = 2 + len(self._strings) * 2
      header = struct.pack('<BB', length, usb_constants.DescriptorType.STRING)
      lang_codes = [struct.pack('<H', lang)
                    for lang in self._strings.iterkeys()]
      buf = header + ''.join(lang_codes)
      assert len(buf) == length
      return buf[:length]
    elif lang not in self._strings:
      return None
    elif index not in self._strings[lang]:
      return None
    else:
      string = self._strings[lang][index].encode('UTF-16LE')
      header = struct.pack(
          '<BB', 2 + len(string), usb_constants.DescriptorType.STRING)
      buf = header + string
      return buf[:length]

  def SetConfiguration(self, index):
    """Handle a SET_CONFIGURATION request from the host.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.7.

    Args:
      index: Configuration index selected.

    Returns:
      True on success, None on error to stall the pipe.
    """
    print 'SetConfiguration({})'.format(index)

    for endpoint_addrs in self._active_endpoints.values():
      for endpoint_addr in endpoint_addrs:
        self._chip.StopEndpoint(endpoint_addr)
      endpoint_addrs.clear()

    if index == 0:
      # SET_CONFIGRATION(0) puts the device into the Address state which
      # Windows does before suspending the port.
      return True
    elif index != 1:
      return None

    config_desc = self.GetConfigurationDescriptor()
    for interface_desc in config_desc.GetInterfaces():
      if interface_desc.bAlternateSetting != 0:
        continue
      endpoint_addrs = self._active_endpoints.setdefault(
          interface_desc.bInterfaceNumber, set())
      for endpoint_desc in interface_desc.GetEndpoints():
        self._chip.StartEndpoint(endpoint_desc)
        endpoint_addrs.add(endpoint_desc.bEndpointAddress)
    return True

  def SetInterface(self, interface, alt_setting):
    """Handle a SET_INTERFACE request from the host.

    See Universal Serial Bus Specification Revision 2.0 section 9.4.10.

    Args:
      interface: Interface number to configure.
      alt_setting: Alternate setting to select.

    Returns:
      True on success, None on error to stall the pipe.
    """
    print 'SetInterface({}, {})'.format(interface, alt_setting)

    config_desc = self.GetConfigurationDescriptor()
    interface_desc = None
    for interface_option in config_desc.GetInterfaces():
      if (interface_option.bInterfaceNumber == interface and
          interface_option.bAlternateSetting == alt_setting):
        interface_desc = interface_option
    if interface_desc is None:
      return None

    endpoint_addrs = self._active_endpoints.setdefault(interface, set())
    for endpoint_addr in endpoint_addrs:
      self._chip.StopEndpoint(endpoint_addr)
    for endpoint_desc in interface_desc.GetEndpoints():
      self._chip.StartEndpoint(endpoint_desc)
      endpoint_addrs.add(endpoint_desc.bEndpointAddress)
    return True
