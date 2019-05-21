# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Misc utils.

Misc utils for interpreting protocols.
"""

from __future__ import absolute_import


def grab_interface_messages(interface):
  """Get the events+requests in this interface.

  Args:
    interface: the interface which you want the messages of.

  Yields:
    All the events followed by all the requests.
  """
  for e in interface.findall('event'):
    yield  e
  for r in interface.findall('request'):
    yield r


def is_constructor(message):
  """Check if a message is a constructor.

  Args:
    message: the message which you want to check.

  Returns:
    True if the message constructs an object (via new_id), False otherwise.
  """
  return any(['type' in arg.attrib and arg.attrib['type'] == 'new_id'
              for arg in message.findall('arg')])


def is_destructor(message):
  """Check if a message is a destructor.

  Args:
    message: the message which you want to check.

  Returns:
    True if the message destroys its "this" object (i.e. it has a
    type=destructor attribute).
  """
  return 'type' in message.attrib and message.attrib['type'] == 'destructor'

