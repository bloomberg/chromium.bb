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
  def setUp(self):
    super_mox.SuperMoxTestBase.setUp(self)
    self.mox.StubOutWithMock(revert, 'gcl')
    self.mox.StubOutWithMock(revert, 'gclient')
    self.mox.StubOutWithMock(revert, 'gclient_scm')
    self.mox.StubOutWithMock(revert, 'os')
    self.mox.StubOutWithMock(revert.os, 'path')
    self.mox.StubOutWithMock(revert.sys, 'stdout')

    # These functions are not tested.
    self.mox.StubOutWithMock(revert, 'GetRepoBase')
    self.mox.StubOutWithMock(revert, 'CaptureSVNLog')


class RevertUnittest(RevertTestsBase):
  """General revert.py tests."""
  def testMembersChanged(self):
    members = [
      'CaptureSVNLog', 'GetRepoBase', 'Main', 'ModifiedFile', 'NoBlameList',
      'NoModifiedFile', 'OutsideOfCheckout', 'Revert', 'UniqueFast',
      'exceptions', 'gcl', 'gclient', 'gclient_scm', 'gclient_utils',
      'optparse', 'os', 'sys', 'xml'
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(revert, members)


class RevertMainUnittest(RevertTestsBase):
  def setUp(self):
    RevertTestsBase.setUp(self)
    self.mox.StubOutWithMock(revert, 'gcl')
    self.mox.StubOutWithMock(revert, 'os')
    self.mox.StubOutWithMock(revert.os, 'path')
    self.mox.StubOutWithMock(revert, 'sys')
    self.mox.StubOutWithMock(revert, 'Revert')

  def testMain(self):
    revert.gcl.GetInfoDir().AndReturn('foo')
    revert.os.path.exists('foo').AndReturn(True)
    revert.Revert([42, 23], True, True, False, 'bleh', ['foo@example.com']
        ).AndReturn(31337)
    self.mox.ReplayAll()

    self.assertEquals(revert.Main(['revert', '-c', '-f', '-n', '-m', 'bleh',
                                   '-r', 'foo@example.com', '42', '23']),
                      31337)


class RevertRevertUnittest(RevertTestsBase):
  def setUp(self):
    RevertTestsBase.setUp(self)

  def testRevert(self):
    revert.gcl.GetRepositoryRoot().AndReturn('foo')
    revert.os.chdir('foo')
    entries = [{
      'author': 'Georges',
      'paths': [
          {'path': 'proto://fqdn/repo/random_file'}
      ],
    }]
    revert.CaptureSVNLog(['-r', '42', '-v']).AndReturn(entries)
    revert.GetRepoBase().AndReturn('proto://fqdn/repo/')
    revert.gclient_scm.CaptureSVNStatus(['random_file']).AndReturn([])
    revert.gcl.RunShell(['svn', 'up', 'random_file'])
    revert.os.path.isdir('random_file').AndReturn(False)
    status = """--- Reverse-merging r42 into '.':
M    random_file
"""
    revert.gcl.RunShellWithReturnCode(['svn', 'merge', '-c', '-42',
                                       'random_file'],
                                      print_output=True).AndReturn([status, 0])
    change = self.mox.CreateMockAnything()
    revert.gcl.ChangeInfo('revert42', 0, 0, 'Reverting 42.\n\nbleh',
                          [('M      ', 'random_file')], 'foo').AndReturn(change)
    change.Save()
    revert.gcl.UploadCL(change,
                        ['--no_presubmit', '-r', 'foo@example.com', '--no_try'])
    revert.gcl.Commit(change, ['--no_presubmit', '--force'])
    revert.gclient.Main(['gclient.py', 'sync'])
    outputs = [
      'Blaming Georges\n',
      'Emailing foo@example.com\n',
      'These files were modified in 42:',
      'random_file',
      '',
      'Reverting 42 in ./'
    ]
    for line in outputs:
      revert.sys.stdout.write(line)
      revert.sys.stdout.write('\n')
    self.mox.ReplayAll()

    revert.Revert([42], True, True, False, 'bleh', ['foo@example.com'])


if __name__ == '__main__':
  unittest.main()
