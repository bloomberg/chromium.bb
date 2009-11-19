#!/usr/bin/python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import StringIO

import gclient_utils
from super_mox import SuperMoxTestBase

class SubprocessCallAndFilterTestCase(SuperMoxTestBase):
  def testSubprocessCallAndFilter(self):
    command = ['boo', 'foo', 'bar']
    in_directory = 'bleh'
    fail_status = None
    pattern = 'a(.*)b'
    test_string = 'ahah\naccb\nallo\naddb\n'
    class Mock(object):
      stdout = StringIO.StringIO(test_string)
      def wait(self):
        pass
    kid = Mock()
    print("\n________ running 'boo foo bar' in 'bleh'")
    for i in test_string:
      gclient_utils.sys.stdout.write(i)
    gclient_utils.subprocess.Popen(
        command, bufsize=0, cwd=in_directory,
        shell=(gclient_utils.sys.platform == 'win32'),
        stdout=gclient_utils.subprocess.PIPE,
        stderr=gclient_utils.subprocess.STDOUT).AndReturn(kid)
    self.mox.ReplayAll()
    compiled_pattern = re.compile(pattern)
    line_list = []
    capture_list = []
    def FilterLines(line):
      line_list.append(line)
      match = compiled_pattern.search(line)
      if match:
        capture_list.append(match.group(1))
    gclient_utils.SubprocessCallAndFilter(
        command, in_directory,
        True, True,
        fail_status, FilterLines)
    self.assertEquals(line_list, ['ahah', 'accb', 'allo', 'addb'])
    self.assertEquals(capture_list, ['cc', 'dd'])


class SplitUrlRevisionTestCase(SuperMoxTestBase):
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
