# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simplify unit tests based on pymox."""

from __future__ import print_function

import os
import random
import shutil
import string
import subprocess
import sys

sys.path.append(os.path.dirname(os.path.dirname(__file__)))
import mox
from third_party.six.moves import StringIO
from testing_support.test_case_utils import TestCaseUtils


class IsOneOf(mox.Comparator):
  def __init__(self, keys):
    self._keys = keys

  def equals(self, rhs):
    return rhs in self._keys

  def __repr__(self):
    return '<sequence or map containing \'%s\'>' % str(self._keys)


class StdoutCheck(object):
  def setUp(self):
    # Override the mock with a StringIO, it's much less painful to test.
    self._old_stdout = sys.stdout
    stdout = StringIO()
    stdout.flush = lambda: None
    sys.stdout = stdout

  def tearDown(self):
    try:
      # If sys.stdout was used, self.checkstdout() must be called.
      # pylint: disable=no-member
      if not sys.stdout.closed:
        self.assertEqual('', sys.stdout.getvalue())
    except AttributeError:
      pass
    sys.stdout = self._old_stdout

  def checkstdout(self, expected):
    value = sys.stdout.getvalue()
    sys.stdout.close()
    # pylint: disable=no-member
    self.assertEqual(expected, value)


class SuperMoxTestBase(TestCaseUtils, StdoutCheck, mox.MoxTestBase):
  def setUp(self):
    """Patch a few functions with know side-effects."""
    TestCaseUtils.setUp(self)
    mox.MoxTestBase.setUp(self)
    os_to_mock = ('chdir', 'chown', 'close', 'closerange', 'dup', 'dup2',
      'fchdir', 'fchmod', 'fchown', 'fdopen', 'getcwd', 'listdir', 'lseek',
      'makedirs', 'mkdir', 'open', 'popen', 'popen2', 'popen3', 'popen4',
      'read', 'remove', 'removedirs', 'rename', 'renames', 'rmdir', 'symlink',
      'system', 'tmpfile', 'walk', 'write')
    self.MockList(os, os_to_mock)
    os_path_to_mock = ('abspath', 'exists', 'getsize', 'isdir', 'isfile',
      'islink', 'ismount', 'lexists', 'realpath', 'samefile', 'walk')
    self.MockList(os.path, os_path_to_mock)
    self.MockList(shutil, ('rmtree'))
    self.MockList(subprocess, ('call', 'Popen'))
    # Don't mock stderr since it confuses unittests.
    self.MockList(sys, ('stdin'))
    StdoutCheck.setUp(self)

  def tearDown(self):
    StdoutCheck.tearDown(self)
    mox.MoxTestBase.tearDown(self)

  def MockList(self, parent, items_to_mock):
    for item in items_to_mock:
      # Skip over items not present because of OS-specific implementation,
      # implemented only in later python version, etc.
      if hasattr(parent, item):
        try:
          self.mox.StubOutWithMock(parent, item)
        except TypeError as e:
          raise TypeError(
              'Couldn\'t mock %s in %s: %s' % (item, parent.__name__, e))

  def UnMock(self, obj, name):
    """Restore an object inside a test."""
    for (parent, old_child, child_name) in self.mox.stubs.cache:
      if parent == obj and child_name == name:
        setattr(parent, child_name, old_child)
        break
