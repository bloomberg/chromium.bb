#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Smoke tests for gclient.py.

Shell out 'gclient' and run basic conformance tests.

This test assumes GClientSmokeBase.URL_BASE is valid.
"""

import os
import shutil
import subprocess
import sys
import unittest

from fake_repos import rmtree, FakeRepos

SHOULD_LEAK = False
UNITTEST_DIR = os.path.abspath(os.path.dirname(__file__))
GCLIENT_PATH = os.path.join(os.path.dirname(UNITTEST_DIR), 'gclient')
# all tests outputs goes there.
TRIAL_DIR = os.path.join(UNITTEST_DIR, '_trial')
# In case you want to use another machine to create the fake repos, e.g.
# not on Windows.
HOST = '127.0.0.1'


class GClientSmokeBase(unittest.TestCase):
  # This subversion repository contains a test repository.
  ROOT_DIR = os.path.join(TRIAL_DIR, 'smoke')

  def setUp(self):
    # Vaguely inspired by twisted.
    # Make sure it doesn't try to auto update when testing!
    self.env = os.environ.copy()
    self.env['DEPOT_TOOLS_UPDATE'] = '0'
    # Remove left overs
    self.root_dir = os.path.join(self.ROOT_DIR, self.id())
    rmtree(self.root_dir)
    if not os.path.exists(self.ROOT_DIR):
      os.mkdir(self.ROOT_DIR)
    os.mkdir(self.root_dir)
    self.svn_base = 'svn://%s/svn/' % HOST
    self.git_base = 'git://%s/git/' % HOST

  def tearDown(self):
    if not SHOULD_LEAK:
      rmtree(self.root_dir)

  def gclient(self, cmd, cwd=None):
    if not cwd:
      cwd = self.root_dir
    process = subprocess.Popen([GCLIENT_PATH] + cmd, cwd=cwd, env=self.env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        shell=sys.platform.startswith('win'))
    (stdout, stderr) = process.communicate()
    return (stdout, stderr, process.returncode)

  def check(self, expected, results):
    def checkString(expected, result):
      if expected != result:
        while expected and result and expected[0] == result[0]:
          expected = expected[1:]
          result = result[1:]
      self.assertEquals(expected, result)
    checkString(expected[0], results[0])
    checkString(expected[1], results[1])
    self.assertEquals(expected[2], results[2])


class GClientSmoke(GClientSmokeBase):
  def testCommands(self):
    """This test is to make sure no new command was added."""
    result = self.gclient(['help'])
    self.assertEquals(3189, len(result[0]))
    self.assertEquals(0, len(result[1]))
    self.assertEquals(0, result[2])

  def testNotConfigured(self):
    res = ("", "Error: client not configured; see 'gclient config'\n", 1)
    self.check(res, self.gclient(['cleanup']))
    self.check(res, self.gclient(['diff']))
    self.check(res, self.gclient(['export', 'foo']))
    self.check(res, self.gclient(['pack']))
    self.check(res, self.gclient(['revert']))
    self.check(res, self.gclient(['revinfo']))
    self.check(res, self.gclient(['runhooks']))
    self.check(res, self.gclient(['status']))
    self.check(res, self.gclient(['sync']))
    self.check(res, self.gclient(['update']))


class GClientSmokeSync(GClientSmokeBase):
  """sync is the most important command. Hence test it more."""
  def testSyncSvn(self):
    """Test pure gclient svn checkout, example of Chromium checkout"""
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    results = self.gclient(['sync'])
    self.assertEquals(0, results[2])
    results = self.gclient(['sync', '--revision', 'a@32'])
    self.assertEquals(0, results[2])

  def testSyncGit(self):
    """Test pure gclient git checkout, example of Chromium OS checkout"""
    self.gclient(['config', self.git_base + 'repo_1'])
    results = self.gclient(['sync'])
    print results[0]
    print results[1]
    self.assertEquals(0, results[2])


class GClientSmokeRevert(GClientSmokeBase):
  """revert is the second most important command. Hence test it more."""
  def setUp(self):
    GClientSmokeBase.setUp(self)
    self.gclient(['config', self.URL_BASE])


class GClientSmokeRevInfo(GClientSmokeBase):
  """revert is the second most important command. Hence test it more."""
  def setUp(self):
    GClientSmokeBase.setUp(self)
    self.gclient(['config', self.URL_BASE])


if __name__ == '__main__':
  fake = FakeRepos(TRIAL_DIR, SHOULD_LEAK, True)
  try:
    fake.setUp()
    unittest.main()
  finally:
    fake.tearDown()
