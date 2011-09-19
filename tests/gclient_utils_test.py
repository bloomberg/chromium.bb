#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=E1101,W0403

import os
import StringIO

# Fixes include path.
from super_mox import SuperMoxTestBase
import trial_dir

import gclient_utils
import subprocess2


class GclientUtilBase(SuperMoxTestBase):
  def setUp(self):
    super(GclientUtilBase, self).setUp()
    gclient_utils.sys.stdout.flush = lambda: None
    self.mox.StubOutWithMock(subprocess2, 'Popen')
    self.mox.StubOutWithMock(subprocess2, 'communicate')


class GclientUtilsUnittest(GclientUtilBase):
  """General gclient_utils.py tests."""
  def testMembersChanged(self):
    members = [
        'CheckCallAndFilter',
        'CheckCallAndFilterAndHeader', 'Error', 'ExecutionQueue', 'FileRead',
        'FileWrite', 'FindFileUpwards', 'FindGclientRoot',
        'GetGClientRootAndEntries', 'IsDateRevision', 'MakeDateRevision',
        'MakeFileAutoFlush', 'MakeFileAnnotated', 'PathDifference',
        'PrintableObject', 'RemoveDirectory', 'SoftClone', 'SplitUrlRevision',
        'SyntaxErrorToError', 'WorkItem',
        'errno', 'lockedmethod', 'logging', 'os', 'Queue', 're', 'rmtree',
        'stat', 'subprocess2', 'sys','threading', 'time',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gclient_utils, members)



class CheckCallAndFilterTestCase(GclientUtilBase):
  class ProcessIdMock(object):
    def __init__(self, test_string):
      self.stdout = StringIO.StringIO(test_string)
    def wait(self):
      pass

  def _inner(self, args, test_string):
    cwd = 'bleh'
    gclient_utils.sys.stdout.write(
        '\n________ running \'boo foo bar\' in \'bleh\'\n')
    for i in test_string:
      gclient_utils.sys.stdout.write(i)
    subprocess2.Popen(
        args,
        cwd=cwd,
        stdout=subprocess2.PIPE,
        stderr=subprocess2.STDOUT,
        bufsize=0).AndReturn(self.ProcessIdMock(test_string))

    self.mox.ReplayAll()
    compiled_pattern = gclient_utils.re.compile(r'a(.*)b')
    line_list = []
    capture_list = []
    def FilterLines(line):
      line_list.append(line)
      assert isinstance(line, str), type(line)
      match = compiled_pattern.search(line)
      if match:
        capture_list.append(match.group(1))
    gclient_utils.CheckCallAndFilterAndHeader(
        args, cwd=cwd, always=True, filter_fn=FilterLines)
    self.assertEquals(line_list, ['ahah', 'accb', 'allo', 'addb'])
    self.assertEquals(capture_list, ['cc', 'dd'])

  def testCheckCallAndFilter(self):
    args = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb\n'
    self._inner(args, test_string)
    self.checkstdout('\n________ running \'boo foo bar\' in \'bleh\'\n'
        'ahah\naccb\nallo\naddb\n\n'
        '________ running \'boo foo bar\' in \'bleh\'\nahah\naccb\nallo\naddb'
        '\n')

  def testNoLF(self):
    # Exactly as testCheckCallAndFilterAndHeader without trailing \n
    args = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb'
    self._inner(args, test_string)
    self.checkstdout('\n________ running \'boo foo bar\' in \'bleh\'\n'
        'ahah\naccb\nallo\naddb\n'
        '________ running \'boo foo bar\' in \'bleh\'\nahah\naccb\nallo\naddb')


class SplitUrlRevisionTestCase(GclientUtilBase):
  def testSSHUrl(self):
    url = "ssh://test@example.com/test.git"
    rev = "ac345e52dc"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)
    url = "ssh://example.com/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)
    url = "ssh://example.com/git/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)
    rev = "test-stable"
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)
    url = "ssh://user-name@example.com/~/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)
    url = "ssh://user-name@example.com/~username/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)

  def testSVNUrl(self):
    url = "svn://example.com/test"
    rev = "ac345e52dc"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)


class GClientUtilsTest(trial_dir.TestCase):
  def testHardToDelete(self):
    # Use the fact that tearDown will delete the directory to make it hard to do
    # so.
    l1 = os.path.join(self.root_dir, 'l1')
    l2 = os.path.join(l1, 'l2')
    l3 = os.path.join(l2, 'l3')
    f3 = os.path.join(l3, 'f3')
    os.mkdir(l1)
    os.mkdir(l2)
    os.mkdir(l3)
    gclient_utils.FileWrite(f3, 'foo')
    os.chmod(f3, 0)
    os.chmod(l3, 0)
    os.chmod(l2, 0)
    os.chmod(l1, 0)


if __name__ == '__main__':
  import unittest
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
