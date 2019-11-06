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


def is_event(message):
  return message.tag == 'event'


def is_request(message):
  return message.tag == 'request'


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


def all_interfaces(protocols):
  """Get the interfaces in these protocols.

  Args:
    protocols: the list of protocols you want the interfaces of.

  Yields:
    Tuples (p, i) of (p)rotocol (i)nterface.
  """
  for p in protocols:
    for i in p.findall('interface'):
      yield (p, i)


def all_messages(protocols):
  """Get the messages in these protocols.

  Args:
    protocols: the list of protocols you want the messages of.

  Yields:
    Tuples (p, i, m) of (p)rotocol, (i)nterface, and (m)essage.
  """
  for (p, i) in all_interfaces(protocols):
    for m in grab_interface_messages(i):
      yield (p, i, m)


def get_constructed(message):
  """Gets the interface constructed by a message.

  Note that even if is_constructor(message) returns true, get_constructed can
  still return None when the message constructs an unknown interface (e.g.
  wl_registry.bind()).

  Args:
    message: the message which may be a constructor.

  Returns:
    The name of the constructed interface (if there is one), or None.
  """
  for arg in message.findall('arg'):
    if 'type' in arg.attrib and arg.attrib['type'] == 'new_id':
      return arg.attrib.get('interface', None)
  return None


def get_globals(protocols):
  """List all of the global interfaces (i.e. those without a constructor).

  Args:
    protocols: the list of protocols you want the globals for.

  Yields:
    Tuples (p, i) of (p)rotocol, (i)nterface, where the interface is a global.
  """
  non_globals = set(get_constructed(m)
                    for (p, i, m) in all_messages(protocols)
                    if get_constructed(m))
  for (p, i) in all_interfaces(protocols):
    if i.attrib['name'] not in non_globals:
      yield (p, i)

