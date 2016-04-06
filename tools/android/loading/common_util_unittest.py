# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import common_util


class SerializeAttributesTestCase(unittest.TestCase):
  class Foo(object):
    def __init__(self, foo_fighters, whisky_bar):
      # Pylint doesn't like foo and bar, but I guess musical references are OK.
      self.foo_fighters = foo_fighters
      self.whisky_bar = whisky_bar

  def testSerialization(self):
    foo_fighters = self.Foo('1', 2)
    json_dict = common_util.SerializeAttributesToJsonDict(
        {}, foo_fighters, ['foo_fighters', 'whisky_bar'])
    self.assertDictEqual({'foo_fighters': '1', 'whisky_bar': 2}, json_dict)
    # Partial update
    json_dict = common_util.SerializeAttributesToJsonDict(
        {'baz': 42}, foo_fighters, ['whisky_bar'])
    self.assertDictEqual({'baz': 42, 'whisky_bar': 2}, json_dict)
    # Non-existing attribute.
    with self.assertRaises(AttributeError):
      json_dict = common_util.SerializeAttributesToJsonDict(
          {}, foo_fighters, ['foo_fighters', 'whisky_bar', 'baz'])

  def testDeserialization(self):
    foo_fighters = self.Foo('hello', 'world')
    json_dict = {'foo_fighters': 12, 'whisky_bar': 42}
    # Partial.
    foo_fighters = common_util.DeserializeAttributesFromJsonDict(
        json_dict, foo_fighters, ['foo_fighters'])
    self.assertEqual(12, foo_fighters.foo_fighters)
    self.assertEqual('world', foo_fighters.whisky_bar)
    # Complete.
    foo_fighters = common_util.DeserializeAttributesFromJsonDict(
        json_dict, foo_fighters, ['foo_fighters', 'whisky_bar'])
    self.assertEqual(42, foo_fighters.whisky_bar)
    # Non-existing attribute.
    with self.assertRaises(AttributeError):
      json_dict['baz'] = 'bad'
      foo_fighters = common_util.DeserializeAttributesFromJsonDict(
          json_dict, foo_fighters, ['foo_fighters', 'whisky_bar', 'baz'])


if __name__ == '__main__':
  unittest.main()
