# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cros_collections module."""

from __future__ import print_function

import collections

from chromite.lib import cros_collections
from chromite.lib import cros_test_lib


class CollectionTest(cros_test_lib.TestCase):
  """Tests for Collection helper."""

  def testDefaults(self):
    """Verify default values kick in."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    self.assertEqual(o.a, 0)
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, {})

  def testOverrideDefaults(self):
    """Verify we can set custom values at instantiation time."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O(a=1000)
    self.assertEqual(o.a, 1000)
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, {})

  def testSetNoNewMembers(self):
    """Verify we cannot add new members after the fact."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()

    # Need the func since self.assertRaises evaluates the args in this scope.
    def _setit(collection):
      collection.does_not_exit = 10
    self.assertRaises(AttributeError, _setit, o)
    self.assertRaises(AttributeError, setattr, o, 'new_guy', 10)

  def testGetNoNewMembers(self):
    """Verify we cannot get new members after the fact."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()

    # Need the func since self.assertRaises evaluates the args in this scope.
    def _getit(collection):
      return collection.does_not_exit
    self.assertRaises(AttributeError, _getit, o)
    self.assertRaises(AttributeError, getattr, o, 'foooo')

  def testNewValue(self):
    """Verify we change members correctly."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    o.a = 'a string'
    o.c = 123
    self.assertEqual(o.a, 'a string')
    self.assertEqual(o.b, 'string')
    self.assertEqual(o.c, 123)

  def testString(self):
    """Make sure the string representation is readable by da hue mans."""
    O = cros_collections.Collection('O', a=0, b='string', c={})
    o = O()
    self.assertEqual("Collection_O(a=0, b='string', c={})", str(o))


class TestGroupByKey(cros_test_lib.TestCase):
  """Test SplitByKey."""

  def testEmpty(self):
    self.assertEqual({}, cros_collections.GroupByKey([], ''))

  def testGroupByKey(self):
    input_iter = [{'a': None, 'b': 0},
                  {'a': 1, 'b': 1},
                  {'a': 2, 'b': 2},
                  {'a': 1, 'b': 3}]
    expected_result = {
        None: [{'a': None, 'b': 0}],
        1:    [{'a': 1, 'b': 1},
               {'a': 1, 'b': 3}],
        2:    [{'a': 2, 'b': 2}]}
    self.assertEqual(cros_collections.GroupByKey(input_iter, 'a'),
                     expected_result)


class GroupNamedtuplesByKeyTests(cros_test_lib.TestCase):
  """Tests for GroupNamedtuplesByKey"""

  def testGroupNamedtuplesByKeyWithEmptyInputIter(self):
    """Test GroupNamedtuplesByKey with empty input_iter."""
    self.assertEqual({}, cros_collections.GroupByKey([], ''))

  def testGroupNamedtuplesByKey(self):
    """Test GroupNamedtuplesByKey."""
    TestTuple = collections.namedtuple('TestTuple', ('key1', 'key2'))
    r1 = TestTuple('t1', 'val1')
    r2 = TestTuple('t2', 'val2')
    r3 = TestTuple('t2', 'val2')
    r4 = TestTuple('t3', 'val3')
    r5 = TestTuple('t3', 'val3')
    r6 = TestTuple('t3', 'val3')
    input_iter = [r1, r2, r3, r4, r5, r6]

    expected_result = {
        't1': [r1],
        't2': [r2, r3],
        't3': [r4, r5, r6]}
    self.assertDictEqual(
        cros_collections.GroupNamedtuplesByKey(input_iter, 'key1'),
        expected_result)

    expected_result = {
        'val1': [r1],
        'val2': [r2, r3],
        'val3': [r4, r5, r6]}
    self.assertDictEqual(
        cros_collections.GroupNamedtuplesByKey(input_iter, 'key2'),
        expected_result)

    expected_result = {
        None: [r1, r2, r3, r4, r5, r6]}
    self.assertDictEqual(
        cros_collections.GroupNamedtuplesByKey(input_iter, 'test'),
        expected_result)


class InvertDictionayTests(cros_test_lib.TestCase):
  """Tests for InvertDictionary."""

  def testInvertDictionary(self):
    """Test InvertDictionary."""
    changes = ['change_1', 'change_2', 'change_3', 'change_4']
    slaves = ['slave_1', 'slave_2', 'slave_3', 'slave_4']
    slave_changes_dict = {
        slaves[0]: set(changes[0:1]),
        slaves[1]: set(changes[0:2]),
        slaves[2]: set(changes[2:4]),
        slaves[3]: set()
    }
    change_slaves_dict = cros_collections.InvertDictionary(
        slave_changes_dict)

    expected_dict = {
        changes[0]: set(slaves[0:2]),
        changes[1]: set([slaves[1]]),
        changes[2]: set([slaves[2]]),
        changes[3]: set([slaves[2]])
    }
    self.assertDictEqual(change_slaves_dict, expected_dict)
