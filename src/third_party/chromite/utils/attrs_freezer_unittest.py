# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the attrs_freezer module."""

from __future__ import print_function

import six

from chromite.lib import cros_test_lib
from chromite.utils import attrs_freezer


class FrozenAttributesTest(cros_test_lib.TestCase):
  """Tests FrozenAttributesMixin functionality."""

  class DummyClass(object):
    """Any class that does not override __setattr__."""

  class SetattrClass(object):
    """Class that does override __setattr__."""
    SETATTR_OFFSET = 10
    def __setattr__(self, attr, value):
      """Adjust value here to later confirm that this code ran."""
      object.__setattr__(self, attr, self.SETATTR_OFFSET + value)

  def _TestBasics(self, cls):
    # pylint: disable=attribute-defined-outside-init
    def _Expected(val):
      return getattr(cls, 'SETATTR_OFFSET', 0) + val

    obj = cls()
    obj.a = 1
    obj.b = 2
    self.assertEqual(_Expected(1), obj.a)
    self.assertEqual(_Expected(2), obj.b)

    obj.Freeze()
    self.assertRaises(attrs_freezer.Error, setattr, obj, 'a', 3)
    self.assertEqual(_Expected(1), obj.a)

    self.assertRaises(attrs_freezer.Error, setattr, obj, 'c', 3)
    self.assertFalse(hasattr(obj, 'c'))

  def testFrozenByMetaclass(self):
    """Test attribute freezing with FrozenAttributesClass."""
    @six.add_metaclass(attrs_freezer.Class)
    class DummyByMeta(self.DummyClass):
      """Class that freezes DummyClass using metaclass construct."""

    self._TestBasics(DummyByMeta)

    @six.add_metaclass(attrs_freezer.Class)
    class SetattrByMeta(self.SetattrClass):
      """Class that freezes SetattrClass using metaclass construct."""

    self._TestBasics(SetattrByMeta)

  def testFrozenByMixinFirst(self):
    """Test attribute freezing with Mixin first in hierarchy."""
    class Dummy(attrs_freezer.Mixin, self.DummyClass):
      """Class that freezes DummyClass using mixin construct."""

    self._TestBasics(Dummy)

    class Setattr(attrs_freezer.Mixin, self.SetattrClass):
      """Class that freezes SetattrClass using mixin construct."""

    self._TestBasics(Setattr)

  def testFrozenByMixinLast(self):
    """Test attribute freezing with Mixin last in hierarchy."""
    class Dummy(self.DummyClass, attrs_freezer.Mixin):
      """Class that freezes DummyClass using mixin construct."""

    self._TestBasics(Dummy)

    class Setattr(self.SetattrClass, attrs_freezer.Mixin):
      """Class that freezes SetattrClass using mixin construct."""

    self._TestBasics(Setattr)
