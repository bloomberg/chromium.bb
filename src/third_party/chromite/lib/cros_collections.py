# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite extensions on top of the collections module."""

from __future__ import print_function


def Collection(classname, **kwargs):
  """Create a new class with mutable named members.

  This is like collections.namedtuple, but mutable.  Also similar to the
  python 3.3 types.SimpleNamespace.

  Examples:
    # Declare default values for this new class.
    Foo = cros_build_lib.Collection('Foo', a=0, b=10)
    # Create a new class but set b to 4.
    foo = Foo(b=4)
    # Print out a (will be the default 0) and b (will be 4).
    print('a = %i, b = %i' % (foo.a, foo.b))
  """

  def sn_init(self, **kwargs):
    """The new class's __init__ function."""
    # First verify the kwargs don't have excess settings.
    valid_keys = set(self.__slots__[1:])
    these_keys = set(kwargs.keys())
    invalid_keys = these_keys - valid_keys
    if invalid_keys:
      raise TypeError('invalid keyword arguments for this object: %r' %
                      invalid_keys)

    # Now initialize this object.
    for k in valid_keys:
      setattr(self, k, kwargs.get(k, self.__defaults__[k]))

  def sn_repr(self):
    """The new class's __repr__ function."""
    return '%s(%s)' % (classname, ', '.join(
        '%s=%r' % (k, getattr(self, k)) for k in self.__slots__[1:]))

  # Give the new class a unique name and then generate the code for it.
  classname = 'Collection_%s' % classname
  expr = '\n'.join((
      'class %(classname)s(object):',
      '  __slots__ = ["__defaults__", "%(slots)s"]',
      '  __defaults__ = {}',
  )) % {
      'classname': classname,
      'slots': '", "'.join(sorted(str(k) for k in kwargs)),
  }

  # Create the class in a local namespace as exec requires.
  namespace = {}
  exec expr in namespace  # pylint: disable=exec-used
  new_class = namespace[classname]

  # Bind the helpers.
  new_class.__defaults__ = kwargs.copy()
  new_class.__init__ = sn_init
  new_class.__repr__ = sn_repr

  return new_class


def GroupByKey(input_iter, key):
  """Split an iterable of dicts, based on value of a key.

  GroupByKey([{'a': 1}, {'a': 2}, {'a': 1, 'b': 2}], 'a') =>
    {1: [{'a': 1}, {'a': 1, 'b': 2}], 2: [{'a': 2}]}

  Args:
    input_iter: An iterable of dicts.
    key: A string specifying the key name to split by.

  Returns:
    A dictionary, mapping from each unique value for |key| that
    was encountered in |input_iter| to a list of entries that had
    that value.
  """
  split_dict = dict()
  for entry in input_iter:
    split_dict.setdefault(entry.get(key), []).append(entry)
  return split_dict


def GroupNamedtuplesByKey(input_iter, key):
  """Split an iterable of namedtuples, based on value of a key.

  Args:
    input_iter: An iterable of namedtuples.
    key: A string specifying the key name to split by.

  Returns:
    A dictionary, mapping from each unique value for |key| that
    was encountered in |input_iter| to a list of entries that had
    that value.
  """
  split_dict = {}
  for entry in input_iter:
    split_dict.setdefault(getattr(entry, key, None), []).append(entry)
  return split_dict


def InvertDictionary(origin_dict):
  """Invert the key value mapping in the origin_dict.

  Given an origin_dict {'key1': {'val1', 'val2'}, 'key2': {'val1', 'val3'},
  'key3': {'val3'}}, the returned inverted dict will be
  {'val1': {'key1', 'key2'}, 'val2': {'key1'}, 'val3': {'key2', 'key3'}}

  Args:
    origin_dict: A dict mapping each key to a group (collection) of values.

  Returns:
    An inverted dict mapping each key to a set of its values.
  """
  new_dict = {}
  for origin_key, origin_values in origin_dict.items():
    for origin_value in origin_values:
      new_dict.setdefault(origin_value, set()).add(origin_key)

  return new_dict
