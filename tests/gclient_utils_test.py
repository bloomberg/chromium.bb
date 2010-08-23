#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import StringIO

# Fixes include path.
from super_mox import SuperMoxTestBase

import gclient_utils


class GclientUtilBase(SuperMoxTestBase):
  def setUp(self):
    super(GclientUtilBase, self).setUp()
    gclient_utils.sys.stdout.flush = lambda: None


class GclientUtilsUnittest(GclientUtilBase):
  """General gclient_utils.py tests."""
  def testMembersChanged(self):
    members = [
        'CheckCall', 'CheckCallError', 'Error', 'ExecutionQueue', 'FileRead',
        'FileWrite', 'FindFileUpwards', 'FindGclientRoot',
        'GetGClientRootAndEntries', 'GetNamedNodeText',
        'GetNodeNamedAttributeText', 'PathDifference', 'ParseXML', 'Popen',
        'PrintableObject', 'RemoveDirectory', 'SplitUrlRevision',
        'SubprocessCall', 'SubprocessCallAndFilter', 'SyntaxErrorToError',
        'WorkItem',
        'errno', 'logging', 'os', 're', 'stat', 'subprocess', 'sys',
        'threading', 'time', 'xml',
    ]
    # If this test fails, you should add the relevant test.
    self.compareMembers(gclient_utils, members)


class CheckCallTestCase(GclientUtilBase):
  def testCheckCallSuccess(self):
    command = ['boo', 'foo', 'bar']
    process = self.mox.CreateMockAnything()
    process.returncode = 0
    env = gclient_utils.os.environ.copy()
    env['LANGUAGE'] = 'en'
    gclient_utils.subprocess.Popen(
        command, cwd=None,
        stderr=None,
        env=env,
        stdout=gclient_utils.subprocess.PIPE,
        shell=gclient_utils.sys.platform.startswith('win')).AndReturn(process)
    process.communicate().AndReturn(['bleh', 'foo'])
    self.mox.ReplayAll()
    gclient_utils.CheckCall(command)

  def testCheckCallFailure(self):
    command = ['boo', 'foo', 'bar']
    process = self.mox.CreateMockAnything()
    process.returncode = 42
    env = gclient_utils.os.environ.copy()
    env['LANGUAGE'] = 'en'
    gclient_utils.subprocess.Popen(
        command, cwd=None,
        stderr=None,
        env=env,
        stdout=gclient_utils.subprocess.PIPE,
        shell=gclient_utils.sys.platform.startswith('win')).AndReturn(process)
    process.communicate().AndReturn(['bleh', 'foo'])
    self.mox.ReplayAll()
    try:
      gclient_utils.CheckCall(command)
      self.fail()
    except gclient_utils.CheckCallError, e:
      self.assertEqual(e.command, command)
      self.assertEqual(e.cwd, None)
      self.assertEqual(e.retcode, 42)
      self.assertEqual(e.stdout, 'bleh')
      self.assertEqual(e.stderr, 'foo')


class SubprocessCallAndFilterTestCase(GclientUtilBase):
  class ProcessIdMock(object):
    def __init__(self, test_string):
      self.stdout = StringIO.StringIO(test_string)
    def wait(self):
        pass

  def _inner(self, command, test_string):
    in_directory = 'bleh'
    env = gclient_utils.os.environ.copy()
    env['LANGUAGE'] = 'en'
    print("\n________ running 'boo foo bar' in 'bleh'")
    for i in test_string:
      gclient_utils.sys.stdout.write(i)
    gclient_utils.subprocess.Popen(
        command,
        cwd=in_directory,
        shell=(gclient_utils.sys.platform == 'win32'),
        env=env,
        stdout=gclient_utils.subprocess.PIPE,
        stderr=gclient_utils.subprocess.STDOUT,
        bufsize=0).AndReturn(self.ProcessIdMock(test_string))

    self.mox.ReplayAll()
    compiled_pattern = gclient_utils.re.compile(r'a(.*)b')
    line_list = []
    capture_list = []
    def FilterLines(line):
      line_list.append(line)
      match = compiled_pattern.search(line)
      if match:
        capture_list.append(match.group(1))
    gclient_utils.SubprocessCallAndFilter(
        command, in_directory, True, True, None, FilterLines)
    self.assertEquals(line_list, ['ahah', 'accb', 'allo', 'addb'])
    self.assertEquals(capture_list, ['cc', 'dd'])

  def testSubprocessCallAndFilter(self):
    command = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb\n'
    self._inner(command, test_string)

  def testNoLF(self):
    # Exactly as testSubprocessCallAndFilter without trailing \n
    command = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb'
    self._inner(command, test_string)


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

  def testSVNUrl(self):
    url = "svn://example.com/test"
    rev = "ac345e52dc"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEquals(out_rev, None)
    self.assertEquals(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEquals(out_rev, rev)
    self.assertEquals(out_url, url)


if __name__ == '__main__':
  import unittest
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
