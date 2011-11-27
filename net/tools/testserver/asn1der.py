# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper module for ASN.1/DER encoding."""

import binascii
import struct

# Tags as defined by ASN.1.
INTEGER = 2
BIT_STRING = 3
NULL = 5
OBJECT_IDENTIFIER = 6
SEQUENCE = 0x30

def Data(tag, data):
  """Generic type-length-value encoder.

  Args:
    tag: the tag.
    data: the data for the given tag.
  Returns:
    encoded TLV value.
  """
  if len(data) == 0:
    return struct.pack(">BB", tag, 0);
  assert len(data) <= 0xffff;
  return struct.pack(">BBH", tag, 0x82, len(data)) + data;

def Integer(value):
  """Encodes an integer.

  Args:
    value: the long value.
  Returns:
    encoded TLV value.
  """
  data = '%x' % value
  return Data(INTEGER, binascii.unhexlify('00' + '0' * (len(data) % 2) + data))

def Bitstring(value):
  """Encodes a bit string.

  Args:
    value: a string holding the binary data.
  Returns:
    encoded TLV value.
  """
  return Data(BIT_STRING, '\x00' + value)

def Sequence(values):
  """Encodes a sequence of other values.

  Args:
    values: the list of values, must be strings holding already encoded data.
  Returns:
    encoded TLV value.
  """
  return Data(SEQUENCE, ''.join(values))
