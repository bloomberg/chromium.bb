#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for git_drover."""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support import auto_stub
import git_drover


class GitDroverTest(auto_stub.TestCase):

  def setUp(self):
    super(GitDroverTest, self).setUp()
    self._temp_directory = tempfile.mkdtemp()
    self._parent_repo = os.path.join(self._temp_directory, 'parent_repo')
    self._target_repo = os.path.join(self._temp_directory, 'drover_branch_123')
    os.makedirs(os.path.join(self._parent_repo, '.git'))
    with open(os.path.join(self._parent_repo, '.git', 'config'), 'w') as f:
      f.write('config')
    with open(os.path.join(self._parent_repo, '.git', 'HEAD'), 'w') as f:
      f.write('HEAD')
    os.mkdir(os.path.join(self._parent_repo, '.git', 'info'))
    with open(os.path.join(self._parent_repo, '.git', 'info', 'refs'),
              'w') as f:
      f.write('refs')
    self.mock(tempfile, 'mkdtemp', self._mkdtemp)
    self.mock(__builtins__, 'raw_input', self._get_input)
    self.mock(subprocess, 'check_call', self._check_call)
    self.mock(subprocess, 'check_output', self._check_call)
    self._commands = []
    self._input = []
    self._fail_on_command = None
    self._has_os_symlink = hasattr(os, 'symlink')
    if not self._has_os_symlink:
      os.symlink = lambda source, dest: None

    self.REPO_CHECK_COMMANDS = [
        (['git', '--help'], self._parent_repo),
        (['git', 'status'], self._parent_repo),
        (['git', 'fetch', 'origin'], self._parent_repo),
        (['git', 'rev-parse', 'refs/remotes/branch-heads/branch^{commit}'],
         self._parent_repo),
        (['git', 'rev-parse', 'cl^{commit}'], self._parent_repo),
        (['git', 'show', '-s', 'cl'], self._parent_repo),
    ]
    self.LOCAL_REPO_COMMANDS = [
        (['git', 'rev-parse', '--git-dir'], self._parent_repo),
        (['git', 'config', 'core.sparsecheckout', 'true'], self._target_repo),
        (['git', 'checkout', '-b', 'drover_branch_123',
          'refs/remotes/branch-heads/branch'], self._target_repo),
        (['git', 'cherry-pick', '-x', 'cl'], self._target_repo),
        (['git', 'reset', '--hard'], self._target_repo),
    ]
    self.UPLOAD_COMMAND = [(['git', 'cl', 'upload'], self._target_repo)]
    self.LAND_COMMAND = [
        (['git', 'cl', 'land', '--bypass-hooks'], self._target_repo),
    ]
    self.BRANCH_CLEANUP_COMMANDS = [
        (['git', 'cherry-pick', '--abort'], self._target_repo),
        (['git', 'checkout', '--detach'], self._target_repo),
        (['git', 'branch', '-D', 'drover_branch_123'], self._target_repo),
    ]

  def tearDown(self):
    shutil.rmtree(self._temp_directory)
    if not self._has_os_symlink:
      del os.symlink
    super(GitDroverTest, self).tearDown()

  def _mkdtemp(self, prefix='tmp'):
    self.assertEqual('drover_branch_', prefix)
    os.mkdir(self._target_repo)
    return self._target_repo

  def _get_input(self, message):
    result = self._input.pop(0)
    if result == 'EOF':
      raise EOFError
    return result

  def _check_call(self, args, stderr=None, stdout=None, shell='', cwd=None):
    self.assertFalse(shell)
    self._commands.append((args, cwd))
    if (self._fail_on_command is not None and
        self._fail_on_command == len(self._commands)):
      raise subprocess.CalledProcessError(1, args[0])
    if args == ['git', 'rev-parse', '--git-dir']:
      return os.path.join(self._parent_repo, '.git')

  def testSuccess(self):
    self._input = ['y', 'y']
    git_drover.cherry_pick_change('branch', 'cl', self._parent_repo, False)
    self.assertEqual(
        self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS +
        self.UPLOAD_COMMAND + self.LAND_COMMAND + self.BRANCH_CLEANUP_COMMANDS,
        self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testDryRun(self):
    self._input = ['y']
    git_drover.cherry_pick_change('branch', 'cl', self._parent_repo, True)
    self.assertEqual(
        self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS +
        self.BRANCH_CLEANUP_COMMANDS, self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testCancelEarly(self):
    self._input = ['n']
    git_drover.cherry_pick_change('branch', 'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS, self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testEOFOnConfirm(self):
    self._input = ['EOF']
    git_drover.cherry_pick_change('branch', 'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS, self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testCancelLate(self):
    self._input = ['y', 'n']
    git_drover.cherry_pick_change('branch', 'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS +
                     self.UPLOAD_COMMAND + self.BRANCH_CLEANUP_COMMANDS,
                     self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testFailDuringCheck(self):
    self._input = []
    self._fail_on_command = 1
    self.assertRaises(git_drover.Error, git_drover.cherry_pick_change, 'branch',
                      'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS[:1], self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testFailDuringBranchCreation(self):
    self._input = ['y']
    self._fail_on_command = 8
    self.assertRaises(git_drover.Error, git_drover.cherry_pick_change, 'branch',
                      'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS[:2],
                     self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testFailDuringCherryPick(self):
    self._input = ['y']
    self._fail_on_command = 10
    self.assertRaises(git_drover.Error, git_drover.cherry_pick_change, 'branch',
                      'cl', self._parent_repo, False)
    self.assertEqual(
        self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS[:4] +
        self.BRANCH_CLEANUP_COMMANDS, self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testFailAfterCherryPick(self):
    self._input = ['y']
    self._fail_on_command = 11
    self.assertRaises(git_drover.Error, git_drover.cherry_pick_change, 'branch',
                      'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS +
                     self.BRANCH_CLEANUP_COMMANDS, self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testFailOnUpload(self):
    self._input = ['y']
    self._fail_on_command = 12
    self.assertRaises(git_drover.Error, git_drover.cherry_pick_change, 'branch',
                      'cl', self._parent_repo, False)
    self.assertEqual(self.REPO_CHECK_COMMANDS + self.LOCAL_REPO_COMMANDS +
                     self.UPLOAD_COMMAND + self.BRANCH_CLEANUP_COMMANDS,
                     self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)

  def testInvalidParentRepoDirectory(self):
    self.assertRaises(
        git_drover.Error, git_drover.cherry_pick_change, 'branch', 'cl',
        os.path.join(self._parent_repo, 'fake'), False)
    self.assertFalse(self._commands)
    self.assertFalse(os.path.exists(self._target_repo))
    self.assertFalse(self._input)


if __name__ == '__main__':
  unittest.main()
