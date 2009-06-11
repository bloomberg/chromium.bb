#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for revert.py."""

import os
import unittest

# Local imports
import revert
import super_mox
from super_mox import mox


class RevertTestsBase(super_mox.SuperMoxTestBase):
  """Setups and tear downs the mocks but doesn't test anything as-is."""
  pass


class RevertUnittest(RevertTestsBase):
  """General revert.py tests."""
  def testMembersChanged(self):
    members = [
      'CaptureSVNLog', 'GetRepoBase', 'Main', 'ModifiedFile', 'NoBlameList',
      'NoModifiedFile', 'OutsideOfCheckout', 'Revert', 'UniqueFast',
      'exceptions', 'gcl', 'gclient', 'optparse', 'os', 'sys', 'xml'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(revert, members)


if __name__ == '__main__':
  unittest.main()
