#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simplify unit tests based on pymox."""

import os
import random
import string
from pymox import mox 


class SuperMoxTestBase(mox.MoxTestBase):
  # Backup the separator in case it gets mocked
  _OS_SEP = os.sep
  _RANDOM_CHOICE = random.choice
  _RANDOM_RANDINT = random.randint
  _STRING_LETTERS = string.letters

  ## Some utilities for generating arbitrary arguments.
  def String(self, max_length):
    return ''.join([self._RANDOM_CHOICE(self._STRING_LETTERS)
                    for x in xrange(self._RANDOM_RANDINT(1, max_length))])

  def Strings(self, max_arg_count, max_arg_length):
    return [self.String(max_arg_length) for x in xrange(max_arg_count)]

  def Args(self, max_arg_count=8, max_arg_length=16):
    return self.Strings(max_arg_count,
                        self._RANDOM_RANDINT(1, max_arg_length))

  def _DirElts(self, max_elt_count=4, max_elt_length=8):
    return self._OS_SEP.join(self.Strings(max_elt_count, max_elt_length))

  def Dir(self, max_elt_count=4, max_elt_length=8):
    return (self._RANDOM_CHOICE((self._OS_SEP, '')) +
            self._DirElts(max_elt_count, max_elt_length))

  def Url(self, max_elt_count=4, max_elt_length=8):
    return ('svn://random_host:port/a' +
            self._DirElts(max_elt_count, max_elt_length
                ).replace(self._OS_SEP, '/'))

  def RootDir(self, max_elt_count=4, max_elt_length=8):
    return self._OS_SEP + self._DirElts(max_elt_count, max_elt_length)

  def compareMembers(self, object, members):
    """If you add a member, be sure to add the relevant test!"""
    # Skip over members starting with '_' since they are usually not meant to
    # be for public use.
    actual_members = [x for x in sorted(dir(object))
                      if not x.startswith('_')]
    expected_members = sorted(members)
    if actual_members != expected_members:
      diff = ([i for i in actual_members if i not in expected_members] +
              [i for i in expected_members if i not in actual_members])
      print diff
    self.assertEqual(actual_members, expected_members)
