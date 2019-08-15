#!/usr/bin/env python
# coding=utf-8
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import unicode_literals

import io
import os
import sys
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support import trial_dir
from third_party import mock

import gclient_utils
import subprocess2


class CheckCallAndFilterTestCase(unittest.TestCase):
  class ProcessIdMock(object):
    def __init__(self, test_string, return_code=0):
      self.stdout = io.BytesIO(test_string.encode('utf-8'))
      self.pid = 9284
      self.return_code = return_code

    def wait(self):
      return self.return_code

  def setUp(self):
    super(CheckCallAndFilterTestCase, self).setUp()
    mock.patch('sys.stdout', io.StringIO()).start()
    mock.patch('sys.stdout.flush', lambda: None).start()
    self.addCleanup(mock.patch.stopall)

  @mock.patch('subprocess2.Popen')
  def testCheckCallAndFilter(self, mockPopen):
    cwd = 'bleh'
    args = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb\n✔'

    mockPopen.return_value = self.ProcessIdMock(test_string)

    line_list = []
    result = gclient_utils.CheckCallAndFilter(
        args, cwd=cwd, show_header=True, always_show_header=True,
        filter_fn=line_list.append)

    self.assertEqual(result, test_string.encode('utf-8'))
    self.assertEqual(line_list, [
        '________ running \'boo foo bar\' in \'bleh\'\n',
        'ahah',
        'accb',
        'allo',
        'addb',
        '✔'])

    mockPopen.assert_called_with(
        args, cwd=cwd, stdout=subprocess2.PIPE, stderr=subprocess2.STDOUT,
        bufsize=0)

  @mock.patch('time.sleep')
  @mock.patch('subprocess2.Popen')
  def testCheckCallAndFilter_RetryOnce(self, mockPopen, mockTime):
    cwd = 'bleh'
    args = ['boo', 'foo', 'bar']
    test_string = 'ahah\naccb\nallo\naddb\n✔'

    mockPopen.side_effect = [
        self.ProcessIdMock(test_string, 1),
        self.ProcessIdMock(test_string, 0),
    ]

    line_list = []
    result = gclient_utils.CheckCallAndFilter(
        args, cwd=cwd, show_header=True, always_show_header=True,
        filter_fn=line_list.append, retry=True)

    self.assertEqual(result, test_string.encode('utf-8'))

    self.assertEqual(line_list, [
        '________ running \'boo foo bar\' in \'bleh\'\n',
        'ahah',
        'accb',
        'allo',
        'addb',
        '✔',
        '________ running \'boo foo bar\' in \'bleh\' attempt 2 / 4\n',
        'ahah',
        'accb',
        'allo',
        'addb',
        '✔',
    ])

    mockTime.assert_called_with(gclient_utils.RETRY_INITIAL_SLEEP)

    self.assertEqual(
        mockPopen.mock_calls,
        [
            mock.call(
                args, cwd=cwd, stdout=subprocess2.PIPE,
                stderr=subprocess2.STDOUT, bufsize=0),
            mock.call(
                args, cwd=cwd, stdout=subprocess2.PIPE,
                stderr=subprocess2.STDOUT, bufsize=0),
        ])

    self.assertEqual(
        sys.stdout.getvalue(),
        'WARNING: subprocess \'"boo" "foo" "bar"\' in bleh failed; will retry '
        'after a short nap...\n')


class SplitUrlRevisionTestCase(unittest.TestCase):
  def testSSHUrl(self):
    url = "ssh://test@example.com/test.git"
    rev = "ac345e52dc"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    url = "ssh://example.com/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    url = "ssh://example.com/git/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    rev = "test-stable"
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    url = "ssh://user-name@example.com/~/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    url = "ssh://user-name@example.com/~username/test.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)
    url = "git@github.com:dart-lang/spark.git"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)

  def testSVNUrl(self):
    url = "svn://example.com/test"
    rev = "ac345e52dc"
    out_url, out_rev = gclient_utils.SplitUrlRevision(url)
    self.assertEqual(out_rev, None)
    self.assertEqual(out_url, url)
    out_url, out_rev = gclient_utils.SplitUrlRevision("%s@%s" % (url, rev))
    self.assertEqual(out_rev, rev)
    self.assertEqual(out_url, url)


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

  def testUpgradeToHttps(self):
    values = [
        ['', ''],
        [None, None],
        ['foo', 'https://foo'],
        ['http://foo', 'https://foo'],
        ['foo/', 'https://foo/'],
        ['ssh-svn://foo', 'ssh-svn://foo'],
        ['ssh-svn://foo/bar/', 'ssh-svn://foo/bar/'],
        ['codereview.chromium.org', 'https://codereview.chromium.org'],
        ['codereview.chromium.org/', 'https://codereview.chromium.org/'],
        ['http://foo:10000', 'http://foo:10000'],
        ['http://foo:10000/bar', 'http://foo:10000/bar'],
        ['foo:10000', 'http://foo:10000'],
        ['foo:', 'https://foo:'],
    ]
    for content, expected in values:
      self.assertEqual(
          expected, gclient_utils.UpgradeToHttps(content))

  def testParseCodereviewSettingsContent(self):
    values = [
        ['# bleh\n', {}],
        ['\t# foo : bar\n', {}],
        ['Foo:bar', {'Foo': 'bar'}],
        ['Foo:bar:baz\n', {'Foo': 'bar:baz'}],
        [' Foo : bar ', {'Foo': 'bar'}],
        [' Foo : bar \n', {'Foo': 'bar'}],
        ['a:b\n\rc:d\re:f', {'a': 'b', 'c': 'd', 'e': 'f'}],
        ['an_url:http://value/', {'an_url': 'http://value/'}],
        [
          'CODE_REVIEW_SERVER : http://r/s',
          {'CODE_REVIEW_SERVER': 'https://r/s'}
        ],
        ['VIEW_VC:http://r/s', {'VIEW_VC': 'https://r/s'}],
    ]
    for content, expected in values:
      self.assertEqual(
          expected, gclient_utils.ParseCodereviewSettingsContent(content))


if __name__ == '__main__':
  import unittest
  unittest.main()

# vim: ts=2:sw=2:tw=80:et:
