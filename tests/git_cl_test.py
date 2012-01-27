#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_cl.py."""

import os
import StringIO
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support.auto_stub import TestCase

import git_cl
import subprocess2


class PresubmitMock(object):
  def __init__(self, *args, **kwargs):
    self.reviewers = []
  @staticmethod
  def should_continue():
    return True


class RietveldMock(object):
  def __init__(self, *args, **kwargs):
    pass
  @staticmethod
  def get_description(issue):
    return 'Issue: %d' % issue


class WatchlistsMock(object):
  def __init__(self, _):
    pass
  @staticmethod
  def GetWatchersForPaths(_):
    return ['joe@example.com']


class TestGitCl(TestCase):
  def setUp(self):
    super(TestGitCl, self).setUp()
    self.calls = []
    self._calls_done = 0
    self.mock(subprocess2, 'call', self._mocked_call)
    self.mock(subprocess2, 'check_call', self._mocked_call)
    self.mock(subprocess2, 'check_output', self._mocked_call)
    self.mock(subprocess2, 'communicate', self._mocked_call)
    self.mock(subprocess2, 'Popen', self._mocked_call)
    self.mock(git_cl, 'FindCodereviewSettingsFile', lambda: '')
    self.mock(git_cl, 'ask_for_data', self._mocked_call)
    self.mock(git_cl.breakpad, 'post', self._mocked_call)
    self.mock(git_cl.breakpad, 'SendStack', self._mocked_call)
    self.mock(git_cl.presubmit_support, 'DoPresubmitChecks', PresubmitMock)
    self.mock(git_cl.rietveld, 'Rietveld', RietveldMock)
    self.mock(git_cl.upload, 'RealMain', self.fail)
    self.mock(git_cl.watchlists, 'Watchlists', WatchlistsMock)
    # It's important to reset settings to not have inter-tests interference.
    git_cl.settings = None

  def tearDown(self):
    if not self.has_failed():
      self.assertEquals([], self.calls)
    super(TestGitCl, self).tearDown()

  def _mocked_call(self, *args, **kwargs):
    self.assertTrue(
        self.calls,
        '@%d  Expected: <Missing>   Actual: %r' % (self._calls_done, args))
    expected_args, result = self.calls.pop(0)
    self.assertEquals(
        expected_args,
        args,
        '@%d  Expected: %r   Actual: %r' % (
          self._calls_done, expected_args, args))
    self._calls_done += 1
    return result

  @classmethod
  def _upload_calls(cls):
    return cls._git_base_calls() + cls._git_upload_calls()

  @staticmethod
  def _git_base_calls():
    return [
      ((['git', 'update-index', '--refresh', '-q'],), ''),
      ((['git', 'diff-index', 'HEAD'],), ''),
      ((['git', 'config', 'rietveld.server'],), 'codereview.example.com'),
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'rev-parse', 'HEAD'],), '12345'),
      ((['git', 'diff', '--name-status', '-r', 'master...', '.'],),
        'M\t.gitignore\n'),
      ((['git', 'rev-parse', '--git-dir'],), '.git'),
      ((['git', 'config', 'branch.master.rietveldissue'],), ''),
      ((['git', 'config', 'branch.master.rietveldpatchset'],), ''),
      ((['git', 'log', '--pretty=format:%s%n%n%b', 'master...'],), 'foo'),
      ((['git', 'config', 'user.email'],), 'me@example.com'),
      ((['git', 'diff', '--no-ext-diff', '--stat', '-M', 'master...'],),
       '+dat'),
      ((['git', 'log', '--pretty=format:%s\n\n%b', 'master..'],), 'desc\n'),
    ]

  @staticmethod
  def _git_upload_calls():
    return [
      ((['git', 'config', 'rietveld.cc'],), ''),
      ((['git', 'config', '--get-regexp', '^svn-remote\\.'],),
        (('', None), 0)),
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'svn', 'info'],), ''),
      ((['git', 'config', 'branch.master.rietveldissue', '1'],), ''),
      ((['git', 'config', 'branch.master.rietveldserver',
          'https://codereview.example.com'],), ''),
      ((['git', 'config', 'branch.master.rietveldpatchset', '2'],), ''),
    ]

  @classmethod
  def _dcommit_calls_1(cls):
    return [
      ((['git', 'config', '--get-regexp', '^svn-remote\\.'],),
       ((('svn-remote.svn.url svn://svn.chromium.org/chrome\n'
          'svn-remote.svn.fetch trunk/src:refs/remotes/origin/master'),
         None),
        0)),
      ((['git', 'config', 'rietveld.server'],), 'codereview.example.com'),
      ((['git', 'symbolic-ref', 'HEAD'],), 'refs/heads/working'),
      ((['git', 'config', 'branch.working.merge'],), 'refs/heads/master'),
      ((['git', 'config', 'branch.working.remote'],), 'origin'),
      ((['git', 'update-index', '--refresh', '-q'],), ''),
      ((['git', 'diff-index', 'HEAD'],), ''),
      ((['git', 'rev-list', '^refs/heads/working',
         'refs/remotes/origin/master'],),
         ''),
      ((['git', 'log', '--grep=^git-svn-id:', '-1', '--pretty=format:%H'],),
         '3fc18b62c4966193eb435baabe2d18a3810ec82e'),
      ((['git', 'rev-list', '^3fc18b62c4966193eb435baabe2d18a3810ec82e',
         'refs/remotes/origin/master'],), ''),
    ]

  @classmethod
  def _dcommit_calls_normal(cls):
    return [
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'rev-parse', 'HEAD'],),
          '00ff397798ea57439712ed7e04ab96e13969ef40'),
      ((['git', 'diff', '--name-status', '-r', 'refs/remotes/origin/master...',
         '.'],),
        'M\tPRESUBMIT.py'),
      ((['git', 'rev-parse', '--git-dir'],), '.git'),
      ((['git', 'config', 'branch.working.rietveldissue'],), '12345'),
      ((['git', 'config', 'branch.working.rietveldserver'],),
         'codereview.example.com'),
      ((['git', 'config', 'branch.working.rietveldpatchset'],), '31137'),
      ((['git', 'config', 'user.email'],), 'author@example.com'),
      ((['git', 'config', 'rietveld.tree-status-url'],), ''),
  ]

  @classmethod
  def _dcommit_calls_bypassed(cls):
    return [
      ((['git', 'rev-parse', '--git-dir'],), '.git'),
      ((['git', 'config', 'branch.working.rietveldissue'],), '12345'),
      ((['git', 'config', 'branch.working.rietveldserver'],),
         'codereview.example.com'),
      (('GitClHooksBypassedCommit',
        'Issue https://codereview.example.com/12345 bypassed hook when '
        'committing'), None),
  ]

  @classmethod
  def _dcommit_calls_3(cls):
    return [
      ((['git', 'diff', '--stat', 'refs/remotes/origin/master',
         'refs/heads/working'],),
       (' PRESUBMIT.py |    2 +-\n'
        ' 1 files changed, 1 insertions(+), 1 deletions(-)\n')),
      (('About to commit; enter to confirm.',), None),
      ((['git', 'show-ref', '--quiet', '--verify',
         'refs/heads/git-cl-commit'],),
       (('', None), 0)),
      ((['git', 'branch', '-D', 'git-cl-commit'],), ''),
      ((['git', 'rev-parse', '--show-cdup'],), '\n'),
      ((['git', 'checkout', '-q', '-b', 'git-cl-commit'],), ''),
      ((['git', 'reset', '--soft', 'refs/remotes/origin/master'],), ''),
      ((['git', 'commit', '-m',
         'Issue: 12345\n\nReview URL: https://codereview.example.com/12345'],),
       ''),
      ((['git', 'svn', 'dcommit', '--no-rebase', '--rmdir'],), (('', None), 0)),
      ((['git', 'checkout', '-q', 'working'],), ''),
      ((['git', 'branch', '-D', 'git-cl-commit'],), ''),
  ]

  @staticmethod
  def _cmd_line(description, args):
    """Returns the upload command line passed to upload.RealMain()."""
    msg = description.split('\n', 1)[0]
    return [
        'upload', '--assume_yes', '--server',
        'https://codereview.example.com',
        '--message', msg,
        '--description', description
    ] + args + [
        '--cc', 'joe@example.com',
        'master...'
    ]

  def _run_reviewer_test(
      self,
      upload_args,
      expected_description,
      returned_description,
      final_description,
      reviewers):
    """Generic reviewer test framework."""
    self.calls = self._upload_calls()
    def RunEditor(desc, _):
      self.assertEquals(
          '# Enter a description of the change.\n'
          '# This will displayed on the codereview site.\n'
          '# The first line will also be used as the subject of the review.\n' +
          expected_description,
          desc)
      return returned_description
    self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)
    def check_upload(args):
      self.assertEquals(self._cmd_line(final_description, reviewers), args)
      return 1, 2
    self.mock(git_cl.upload, 'RealMain', check_upload)
    git_cl.main(['upload'] + upload_args)

  def test_no_reviewer(self):
    self._run_reviewer_test(
        [],
        'desc\n\nBUG=\nTEST=\n',
        '# Blah blah comment.\ndesc\n\nBUG=\nTEST=\n',
        'desc\n\nBUG=\nTEST=\n',
        [])

  def test_reviewers_cmd_line(self):
    # Reviewer is passed as-is
    description = 'desc\n\nR=foo@example.com\nBUG=\nTEST=\n'
    self._run_reviewer_test(
        ['-r' 'foo@example.com'],
        description,
        '\n%s\n' % description,
        description,
        ['--reviewers', 'foo@example.com'])

  def test_reviewer_tbr_overriden(self):
    # Reviewer is overriden with TBR
    # Also verifies the regexp work without a trailing LF
    description = 'Foo Bar\nTBR=reviewer@example.com\n'
    self._run_reviewer_test(
        ['-r' 'foo@example.com'],
        'desc\n\nR=foo@example.com\nBUG=\nTEST=\n',
        description.strip('\n'),
        description,
        ['--reviewers', 'reviewer@example.com'])

  def test_reviewer_multiple(self):
    # Handles multiple R= or TBR= lines.
    description = (
        'Foo Bar\nTBR=reviewer@example.com\nBUG=\nR=another@example.com\n')
    self._run_reviewer_test(
        [],
        'desc\n\nBUG=\nTEST=\n',
        description,
        description,
        ['--reviewers', 'reviewer@example.com,another@example.com'])

  def test_reviewer_send_mail(self):
    # --send-mail can be used without -r if R= is used
    description = 'Foo Bar\nR=reviewer@example.com\n'
    self._run_reviewer_test(
        ['--send-mail'],
        'desc\n\nBUG=\nTEST=\n',
        description.strip('\n'),
        description,
        ['--reviewers', 'reviewer@example.com', '--send_mail'])

  def test_reviewer_send_mail_no_rev(self):
    # Fails without a reviewer.
    class FileMock(object):
      buf = StringIO.StringIO()
      def write(self, content):
        self.buf.write(content)

    mock = FileMock()
    try:
      self.calls = self._git_base_calls()
      def RunEditor(desc, _):
        return desc
      self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)
      self.mock(sys, 'stderr', mock)
      git_cl.main(['upload', '--send-mail'])
      self.fail()
    except SystemExit:
      self.assertEquals(
          'Must specify reviewers to send email.\n', mock.buf.getvalue())

  def test_dcommit(self):
    self.calls = (
        self._dcommit_calls_1() +
        self._dcommit_calls_normal() +
        self._dcommit_calls_3())
    git_cl.main(['dcommit'])

  def test_dcommit_bypass_hooks(self):
    self.calls = (
        self._dcommit_calls_1() +
        self._dcommit_calls_bypassed() +
        self._dcommit_calls_3())
    git_cl.main(['dcommit', '--bypass-hooks'])


if __name__ == '__main__':
  unittest.main()
