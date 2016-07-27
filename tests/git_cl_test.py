#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_cl.py."""

import os
import StringIO
import stat
import sys
import unittest
import urlparse

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support.auto_stub import TestCase

import git_cl
import git_common
import git_footers
import subprocess2

class ChangelistMock(object):
  # A class variable so we can access it when we don't have access to the
  # instance that's being set.
  desc = ""
  def __init__(self, **kwargs):
    pass
  def GetIssue(self):
    return 1
  def GetDescription(self):
    return ChangelistMock.desc
  def UpdateDescription(self, desc):
    ChangelistMock.desc = desc

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

  @staticmethod
  def get_issue_properties(_issue, _messages):
    return {
      'reviewers': ['joe@chromium.org', 'john@chromium.org'],
      'messages': [
        {
          'approval': True,
          'sender': 'john@chromium.org',
        },
      ],
    }


class WatchlistsMock(object):
  def __init__(self, _):
    pass
  @staticmethod
  def GetWatchersForPaths(_):
    return ['joe@example.com']


class CodereviewSettingsFileMock(object):
  def __init__(self):
    pass
  # pylint: disable=R0201
  def read(self):
    return ("CODE_REVIEW_SERVER: gerrit.chromium.org\n" +
            "GERRIT_HOST: True\n")


class AuthenticatorMock(object):
  def __init__(self, *_args):
    pass
  def has_cached_credentials(self):
    return True


def CookiesAuthenticatorMockFactory(hosts_with_creds=None, same_cookie=False):
  """Use to mock Gerrit/Git credentials from ~/.netrc or ~/.gitcookies.

  Usage:
    >>> self.mock(git_cl.gerrit_util, "CookiesAuthenticator",
                  CookiesAuthenticatorMockFactory({'host1': 'cookie1'}))

  OR
    >>> self.mock(git_cl.gerrit_util, "CookiesAuthenticator",
                  CookiesAuthenticatorMockFactory(cookie='cookie'))
  """
  class CookiesAuthenticatorMock(git_cl.gerrit_util.CookiesAuthenticator):
    def __init__(self):  # pylint: disable=W0231
      # Intentionally not calling super() because it reads actual cookie files.
      pass
    @classmethod
    def get_gitcookies_path(cls):
      return '~/.gitcookies'
    @classmethod
    def get_netrc_path(cls):
      return '~/.netrc'
    def get_auth_header(self, host):
      if same_cookie:
        return same_cookie
      return (hosts_with_creds or {}).get(host)
  return CookiesAuthenticatorMock


class TestGitClBasic(unittest.TestCase):
  def _test_ParseIssueUrl(self, func, url, issue, patchset, hostname, fail):
    parsed = urlparse.urlparse(url)
    result = func(parsed)
    if fail:
      self.assertIsNone(result)
      return None
    self.assertIsNotNone(result)
    self.assertEqual(result.issue, issue)
    self.assertEqual(result.patchset, patchset)
    self.assertEqual(result.hostname, hostname)
    return result

  def test_ParseIssueURL_rietveld(self):
    def test(url, issue=None, patchset=None, hostname=None, patch_url=None,
             fail=None):
      result = self._test_ParseIssueUrl(
          git_cl._RietveldChangelistImpl.ParseIssueURL,
          url, issue, patchset, hostname, fail)
      if not fail:
        self.assertEqual(result.patch_url, patch_url)

    test('http://codereview.chromium.org/123',
         123, None, 'codereview.chromium.org')
    test('https://codereview.chromium.org/123',
         123, None, 'codereview.chromium.org')
    test('https://codereview.chromium.org/123/',
         123, None, 'codereview.chromium.org')
    test('https://codereview.chromium.org/123/whatever',
         123, None, 'codereview.chromium.org')
    test('http://codereview.chromium.org/download/issue123_4.diff',
         123, 4, 'codereview.chromium.org',
         patch_url='https://codereview.chromium.org/download/issue123_4.diff')
    # This looks like bad Gerrit, but is actually valid Rietveld.
    test('https://chrome-review.source.com/123/4/',
         123, None, 'chrome-review.source.com')

    test('https://codereview.chromium.org/deadbeaf', fail=True)
    test('https://codereview.chromium.org/api/123', fail=True)
    test('bad://codereview.chromium.org/123', fail=True)
    test('http://codereview.chromium.org/download/issue123_4.diffff', fail=True)

  def test_ParseIssueURL_gerrit(self):
    def test(url, issue=None, patchset=None, hostname=None, fail=None):
      self._test_ParseIssueUrl(
          git_cl._GerritChangelistImpl.ParseIssueURL,
          url, issue, patchset, hostname, fail)

    test('http://chrome-review.source.com/c/123',
         123, None, 'chrome-review.source.com')
    test('https://chrome-review.source.com/c/123/',
         123, None, 'chrome-review.source.com')
    test('https://chrome-review.source.com/c/123/4',
         123, 4, 'chrome-review.source.com')
    test('https://chrome-review.source.com/#/c/123/4',
         123, 4, 'chrome-review.source.com')
    test('https://chrome-review.source.com/c/123/4',
         123, 4, 'chrome-review.source.com')
    test('https://chrome-review.source.com/123',
         123, None, 'chrome-review.source.com')
    test('https://chrome-review.source.com/123/4',
         123, 4, 'chrome-review.source.com')

    test('https://chrome-review.source.com/c/123/1/whatisthis', fail=True)
    test('https://chrome-review.source.com/c/abc/', fail=True)
    test('ssh://chrome-review.source.com/c/123/1/', fail=True)

  def test_ParseIssueNumberArgument(self):
    def test(arg, issue=None, patchset=None, hostname=None, fail=False):
      result = git_cl.ParseIssueNumberArgument(arg)
      self.assertIsNotNone(result)
      if fail:
        self.assertFalse(result.valid)
      else:
        self.assertEqual(result.issue, issue)
        self.assertEqual(result.patchset, patchset)
        self.assertEqual(result.hostname, hostname)

    test('123', 123)
    test('', fail=True)
    test('abc', fail=True)
    test('123/1', fail=True)
    test('123a', fail=True)
    test('ssh://chrome-review.source.com/#/c/123/4/', fail=True)
    # Rietveld.
    test('https://codereview.source.com/123',
         123, None, 'codereview.source.com')
    test('https://codereview.source.com/www123', fail=True)
    # Gerrrit.
    test('https://chrome-review.source.com/c/123/4',
         123, 4, 'chrome-review.source.com')
    test('https://chrome-review.source.com/bad/123/4', fail=True)

  def test_get_bug_line_values(self):
    f = lambda p, bugs: list(git_cl._get_bug_line_values(p, bugs))
    self.assertEqual(f('', ''), [])
    self.assertEqual(f('', '123,v8:456'), ['123', 'v8:456'])
    self.assertEqual(f('v8', '456'), ['v8:456'])
    self.assertEqual(f('v8', 'chromium:123,456'), ['v8:456', 'chromium:123'])
    # Not nice, but not worth carying.
    self.assertEqual(f('v8', 'chromium:123,456,v8:123'),
                     ['v8:456', 'chromium:123', 'v8:123'])


class TestGitCl(TestCase):
  def setUp(self):
    super(TestGitCl, self).setUp()
    self.calls = []
    self._calls_done = []
    self.mock(subprocess2, 'call', self._mocked_call)
    self.mock(subprocess2, 'check_call', self._mocked_call)
    self.mock(subprocess2, 'check_output', self._mocked_call)
    self.mock(subprocess2, 'communicate', self._mocked_call)
    self.mock(git_cl.gclient_utils, 'CheckCallAndFilter', self._mocked_call)
    self.mock(git_common, 'is_dirty_git_tree', lambda x: False)
    self.mock(git_common, 'get_or_create_merge_base',
              lambda *a: (
                  self._mocked_call(['get_or_create_merge_base']+list(a))))
    self.mock(git_cl, 'BranchExists', lambda _: True)
    self.mock(git_cl, 'FindCodereviewSettingsFile', lambda: '')
    self.mock(git_cl, 'ask_for_data', self._mocked_call)
    self.mock(git_cl.presubmit_support, 'DoPresubmitChecks', PresubmitMock)
    self.mock(git_cl.rietveld, 'Rietveld', RietveldMock)
    self.mock(git_cl.rietveld, 'CachingRietveld', RietveldMock)
    self.mock(git_cl.upload, 'RealMain', self.fail)
    self.mock(git_cl.watchlists, 'Watchlists', WatchlistsMock)
    self.mock(git_cl.auth, 'get_authenticator_for_host', AuthenticatorMock)
    self.mock(git_cl.gerrit_util.GceAuthenticator, 'is_gce',
              classmethod(lambda _: False))
    # It's important to reset settings to not have inter-tests interference.
    git_cl.settings = None


  def tearDown(self):
    try:
      # Note: has_failed returns True if at least 1 test ran so far, current
      # included, has failed. That means current test may have actually ran
      # fine, and the check for no leftover calls would be skipped.
      if not self.has_failed():
        self.assertEquals([], self.calls)
    finally:
      super(TestGitCl, self).tearDown()

  def _mocked_call(self, *args, **_kwargs):
    self.assertTrue(
        self.calls,
        '@%d  Expected: <Missing>   Actual: %r' % (len(self._calls_done), args))
    top = self.calls.pop(0)
    if len(top) > 2 and top[2]:
      raise top[2]
    expected_args, result = top

    # Also logs otherwise it could get caught in a try/finally and be hard to
    # diagnose.
    if expected_args != args:
      N = 5
      prior_calls  = '\n  '.join(
          '@%d: %r' % (len(self._calls_done) - N + i, c[0])
          for i, c in enumerate(self._calls_done[-N:]))
      following_calls = '\n  '.join(
          '@%d: %r' % (len(self._calls_done) + i + 1, c[0])
          for i, c in enumerate(self.calls[:N]))
      extended_msg = (
          'A few prior calls:\n  %s\n\n'
          'This (expected):\n  @%d: %r\n'
          'This (actual):\n  @%d: %r\n\n'
          'A few following expected calls:\n  %s' %
          (prior_calls, len(self._calls_done), expected_args,
           len(self._calls_done), args, following_calls))
      git_cl.logging.error(extended_msg)

      self.fail('@%d  Expected: %r   Actual: %r' % (
          len(self._calls_done), expected_args, args))

    self._calls_done.append(top)
    return result

  @classmethod
  def _is_gerrit_calls(cls, gerrit=False):
    return [((['git', 'config', 'rietveld.autoupdate'],), ''),
            ((['git', 'config', 'gerrit.host'],), 'True' if gerrit else '')]

  @classmethod
  def _upload_calls(cls, similarity, find_copies, private):
    return (cls._git_base_calls(similarity, find_copies) +
            cls._git_upload_calls(private))

  @classmethod
  def _upload_no_rev_calls(cls, similarity, find_copies):
    return (cls._git_base_calls(similarity, find_copies) +
            cls._git_upload_no_rev_calls())

  @classmethod
  def _git_base_calls(cls, similarity, find_copies):
    if similarity is None:
      similarity = '50'
      similarity_call = ((['git', 'config', '--int', '--get',
                         'branch.master.git-cl-similarity'],), '')
    else:
      similarity_call = ((['git', 'config', '--int',
                         'branch.master.git-cl-similarity', similarity],), '')

    if find_copies is None:
      find_copies = True
      find_copies_call = ((['git', 'config', '--int', '--get',
                          'branch.master.git-find-copies'],), '')
    else:
      val = str(int(find_copies))
      find_copies_call = ((['git', 'config', '--int',
                          'branch.master.git-find-copies', val],), '')

    if find_copies:
      stat_call = ((['git', 'diff', '--no-ext-diff', '--stat',
                   '--find-copies-harder', '-l100000', '-C'+similarity,
                   'fake_ancestor_sha', 'HEAD'],), '+dat')
    else:
      stat_call = ((['git', 'diff', '--no-ext-diff', '--stat',
                   '-M'+similarity, 'fake_ancestor_sha', 'HEAD'],), '+dat')

    return [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      similarity_call,
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      find_copies_call,
    ] + cls._is_gerrit_calls() + [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.rietveldissue'],), ''),
      ((['git', 'config', 'branch.master.gerritissue'],), ''),
      ((['git', 'config', 'rietveld.server'],),
       'codereview.example.com'),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['get_or_create_merge_base', 'master', 'master'],),
       'fake_ancestor_sha'),
    ] + cls._git_sanity_checks('fake_ancestor_sha', 'master') + [
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'rev-parse', 'HEAD'],), '12345'),
      ((['git', 'diff', '--name-status', '--no-renames', '-r',
         'fake_ancestor_sha...', '.'],),
        'M\t.gitignore\n'),
      ((['git', 'config', 'branch.master.rietveldpatchset'],),
       ''),
      ((['git', 'log', '--pretty=format:%s%n%n%b',
         'fake_ancestor_sha...'],),
       'foo'),
      ((['git', 'config', 'user.email'],), 'me@example.com'),
      stat_call,
      ((['git', 'log', '--pretty=format:%s\n\n%b',
         'fake_ancestor_sha..HEAD'],),
       'desc\n'),
      ((['git', 'config', 'rietveld.bug-prefix'],), ''),
    ]

  @classmethod
  def _git_upload_no_rev_calls(cls):
    return [
      ((['git', 'config', 'core.editor'],), ''),
    ]

  @classmethod
  def _git_upload_calls(cls, private):
    if private:
      cc_call = []
      private_call = []
    else:
      cc_call = [((['git', 'config', 'rietveld.cc'],), '')]
      private_call = [
          ((['git', 'config', 'rietveld.private'],), '')]

    return [
        ((['git', 'config', 'core.editor'],), ''),
    ] + cc_call + private_call + [
        ((['git', 'config', 'branch.master.base-url'],), ''),
        ((['git', 'config', 'rietveld.pending-ref-prefix'],), ''),
        ((['git',
           'config', '--local', '--get-regexp', '^svn-remote\\.'],),
         (('', None), 0)),
        ((['git', 'rev-parse', '--show-cdup'],), ''),
        ((['git', 'svn', 'info'],), ''),
        ((['git', 'config', 'rietveld.project'],), ''),
        ((['git',
           'config', 'branch.master.rietveldissue', '1'],), ''),
        ((['git', 'config', 'branch.master.rietveldserver',
           'https://codereview.example.com'],), ''),
        ((['git',
           'config', 'branch.master.rietveldpatchset', '2'],), ''),
    ] + cls._git_post_upload_calls()

  @classmethod
  def _git_post_upload_calls(cls):
    return [
        ((['git', 'rev-parse', 'HEAD'],), 'hash'),
        ((['git', 'symbolic-ref', 'HEAD'],), 'hash'),
        ((['git',
           'config', 'branch.hash.last-upload-hash', 'hash'],), ''),
        ((['git', 'config', 'rietveld.run-post-upload-hook'],), ''),
    ]

  @staticmethod
  def _git_sanity_checks(diff_base, working_branch, get_remote_branch=True):
    fake_ancestor = 'fake_ancestor'
    fake_cl = 'fake_cl_for_patch'
    return [
      ((['git',
         'rev-parse', '--verify', diff_base],), fake_ancestor),
      ((['git',
         'merge-base', fake_ancestor, 'HEAD'],), fake_ancestor),
      ((['git',
         'rev-list', '^' + fake_ancestor, 'HEAD'],), fake_cl),
      # Mock a config miss (error code 1)
      ((['git',
         'config', 'gitcl.remotebranch'],), (('', None), 1)),
    ] + ([
      # Call to GetRemoteBranch()
      ((['git',
         'config', 'branch.%s.merge' % working_branch],),
       'refs/heads/master'),
      ((['git',
         'config', 'branch.%s.remote' % working_branch],), 'origin'),
    ] if get_remote_branch else []) + [
      ((['git', 'rev-list', '^' + fake_ancestor,
         'refs/remotes/origin/master'],), ''),
    ]

  @classmethod
  def _dcommit_calls_1(cls):
    return [
      ((['git', 'config', 'rietveld.autoupdate'],),
       ''),
      ((['git', 'config', 'rietveld.pending-ref-prefix'],),
       ''),
      ((['git',
         'config', '--local', '--get-regexp', '^svn-remote\\.'],),
       ((('svn-remote.svn.url svn://svn.chromium.org/chrome\n'
          'svn-remote.svn.fetch trunk/src:refs/remotes/origin/master'),
         None),
        0)),
      ((['git', 'symbolic-ref', 'HEAD'],), 'refs/heads/working'),
      ((['git', 'config', '--int', '--get',
        'branch.working.git-cl-similarity'],), ''),
      ((['git', 'symbolic-ref', 'HEAD'],), 'refs/heads/working'),
      ((['git', 'config', '--int', '--get',
        'branch.working.git-find-copies'],), ''),
      ((['git', 'symbolic-ref', 'HEAD'],), 'refs/heads/working'),
      ((['git',
         'config', 'branch.working.rietveldissue'],), '12345'),
      ((['git',
         'config', 'rietveld.server'],), 'codereview.example.com'),
      ((['git',
         'config', 'branch.working.merge'],), 'refs/heads/master'),
      ((['git', 'config', 'branch.working.remote'],), 'origin'),
      ((['git', 'config', 'branch.working.merge'],),
       'refs/heads/master'),
      ((['git', 'config', 'branch.working.remote'],), 'origin'),
      ((['git', 'rev-list', '--merges',
         '--grep=^SVN changes up to revision [0-9]*$',
         'refs/remotes/origin/master^!'],), ''),
      ((['git', 'rev-list', '^refs/heads/working',
         'refs/remotes/origin/master'],),
         ''),
      ((['git',
         'log', '--grep=^git-svn-id:', '-1', '--pretty=format:%H'],),
         '3fc18b62c4966193eb435baabe2d18a3810ec82e'),
      ((['git',
         'rev-list', '^3fc18b62c4966193eb435baabe2d18a3810ec82e',
         'refs/remotes/origin/master'],), ''),
      ((['git',
         'merge-base', 'refs/remotes/origin/master', 'HEAD'],),
       'fake_ancestor_sha'),
    ]

  @classmethod
  def _dcommit_calls_normal(cls):
    return [
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'rev-parse', 'HEAD'],),
          '00ff397798ea57439712ed7e04ab96e13969ef40'),
      ((['git',
         'diff', '--name-status', '--no-renames', '-r', 'fake_ancestor_sha...',
         '.'],),
        'M\tPRESUBMIT.py'),
      ((['git',
         'config', 'branch.working.rietveldpatchset'],), '31137'),
      ((['git', 'config', 'branch.working.rietveldserver'],),
         'codereview.example.com'),
      ((['git', 'config', 'user.email'],), 'author@example.com'),
      ((['git', 'config', 'rietveld.tree-status-url'],), ''),
  ]

  @classmethod
  def _dcommit_calls_bypassed(cls):
    return [
      ((['git', 'config', 'branch.working.rietveldserver'],),
         'codereview.example.com'),
  ]

  @classmethod
  def _dcommit_calls_3(cls):
    return [
      ((['git',
         'diff', '--no-ext-diff', '--stat', '--find-copies-harder',
         '-l100000', '-C50', 'fake_ancestor_sha',
         'refs/heads/working'],),
       (' PRESUBMIT.py |    2 +-\n'
        ' 1 files changed, 1 insertions(+), 1 deletions(-)\n')),
      ((['git', 'show-ref', '--quiet', '--verify',
         'refs/heads/git-cl-commit'],),
       (('', None), 0)),
      ((['git', 'branch', '-D', 'git-cl-commit'],), ''),
      ((['git', 'show-ref', '--quiet', '--verify',
         'refs/heads/git-cl-cherry-pick'],), ''),
      ((['git', 'rev-parse', '--show-cdup'],), '\n'),
      ((['git', 'checkout', '-q', '-b', 'git-cl-commit'],), ''),
      ((['git', 'reset', '--soft', 'fake_ancestor_sha'],), ''),
      ((['git', 'commit', '-m',
         'Issue: 12345\n\nR=john@chromium.org\n\n'
         'Review URL: https://codereview.example.com/12345 .'],),
       ''),
      ((['git', 'config', 'rietveld.force-https-commit-url'],), ''),
      ((['git',
         'svn', 'dcommit', '-C50', '--no-rebase', '--rmdir'],),
       (('', None), 0)),
      ((['git', 'checkout', '-q', 'working'],), ''),
      ((['git', 'branch', '-D', 'git-cl-commit'],), ''),
  ]

  @staticmethod
  def _cmd_line(description, args, similarity, find_copies, private):
    """Returns the upload command line passed to upload.RealMain()."""
    return [
        'upload', '--assume_yes', '--server',
        'https://codereview.example.com',
        '--message', description
    ] + args + [
        '--cc', 'joe@example.com',
    ] + (['--private'] if private else []) + [
        '--git_similarity', similarity or '50'
    ] + (['--git_no_find_copies'] if find_copies == False else []) + [
        'fake_ancestor_sha', 'HEAD'
    ]

  def _run_reviewer_test(
      self,
      upload_args,
      expected_description,
      returned_description,
      final_description,
      reviewers,
      private=False):
    """Generic reviewer test framework."""
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    try:
      similarity = upload_args[upload_args.index('--similarity')+1]
    except ValueError:
      similarity = None

    if '--find-copies' in upload_args:
      find_copies = True
    elif '--no-find-copies' in upload_args:
      find_copies = False
    else:
      find_copies = None

    private = '--private' in upload_args

    self.calls = self._upload_calls(similarity, find_copies, private)

    def RunEditor(desc, _, **kwargs):
      self.assertEquals(
          '# Enter a description of the change.\n'
          '# This will be displayed on the codereview site.\n'
          '# The first line will also be used as the subject of the review.\n'
          '#--------------------This line is 72 characters long'
          '--------------------\n' +
          expected_description,
          desc)
      return returned_description
    self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)

    def check_upload(args):
      cmd_line = self._cmd_line(final_description, reviewers, similarity,
                                find_copies, private)
      self.assertEquals(cmd_line, args)
      return 1, 2
    self.mock(git_cl.upload, 'RealMain', check_upload)

    git_cl.main(['upload'] + upload_args)

  def test_no_reviewer(self):
    self._run_reviewer_test(
        [],
        'desc\n\nBUG=',
        '# Blah blah comment.\ndesc\n\nBUG=',
        'desc\n\nBUG=',
        [])

  def test_keep_similarity(self):
    self._run_reviewer_test(
        ['--similarity', '70'],
        'desc\n\nBUG=',
        '# Blah blah comment.\ndesc\n\nBUG=',
        'desc\n\nBUG=',
        [])

  def test_keep_find_copies(self):
    self._run_reviewer_test(
        ['--no-find-copies'],
        'desc\n\nBUG=',
        '# Blah blah comment.\ndesc\n\nBUG=\n',
        'desc\n\nBUG=',
        [])

  def test_private(self):
    self._run_reviewer_test(
        ['--private'],
        'desc\n\nBUG=',
        '# Blah blah comment.\ndesc\n\nBUG=\n',
        'desc\n\nBUG=',
        [])

  def test_reviewers_cmd_line(self):
    # Reviewer is passed as-is
    description = 'desc\n\nR=foo@example.com\nBUG='
    self._run_reviewer_test(
        ['-r' 'foo@example.com'],
        description,
        '\n%s\n' % description,
        description,
        ['--reviewers=foo@example.com'])

  def test_reviewer_tbr_overriden(self):
    # Reviewer is overriden with TBR
    # Also verifies the regexp work without a trailing LF
    description = 'Foo Bar\n\nTBR=reviewer@example.com'
    self._run_reviewer_test(
        ['-r' 'foo@example.com'],
        'desc\n\nR=foo@example.com\nBUG=',
        description.strip('\n'),
        description,
        ['--reviewers=reviewer@example.com'])

  def test_reviewer_multiple(self):
    # Handles multiple R= or TBR= lines.
    description = (
        'Foo Bar\nTBR=reviewer@example.com\nBUG=\nR=another@example.com')
    self._run_reviewer_test(
        [],
        'desc\n\nBUG=',
        description,
        description,
        ['--reviewers=another@example.com,reviewer@example.com'])

  def test_reviewer_send_mail(self):
    # --send-mail can be used without -r if R= is used
    description = 'Foo Bar\nR=reviewer@example.com'
    self._run_reviewer_test(
        ['--send-mail'],
        'desc\n\nBUG=',
        description.strip('\n'),
        description,
        ['--reviewers=reviewer@example.com', '--send_mail'])

  def test_reviewer_send_mail_no_rev(self):
    # Fails without a reviewer.
    stdout = StringIO.StringIO()
    stderr = StringIO.StringIO()
    try:
      self.calls = self._upload_no_rev_calls(None, None)
      def RunEditor(desc, _, **kwargs):
        return desc
      self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)
      self.mock(sys, 'stdout', stdout)
      self.mock(sys, 'stderr', stderr)
      git_cl.main(['upload', '--send-mail'])
      self.fail()
    except SystemExit:
      self.assertEqual(
          'Using 50% similarity for rename/copy detection. Override with '
          '--similarity.\n',
          stdout.getvalue())
      self.assertEqual(
          'Must specify reviewers to send email.\n', stderr.getvalue())

  def test_bug_on_cmd(self):
    self._run_reviewer_test(
        ['--bug=500658,proj:123'],
        'desc\n\nBUG=500658\nBUG=proj:123',
        '# Blah blah comment.\ndesc\n\nBUG=500658\nBUG=proj:1234',
        'desc\n\nBUG=500658\nBUG=proj:1234',
        [])

  def test_dcommit(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = (
        self._dcommit_calls_1() +
        self._git_sanity_checks('fake_ancestor_sha', 'working') +
        self._dcommit_calls_normal() +
        self._dcommit_calls_3())
    git_cl.main(['dcommit'])

  def test_dcommit_bypass_hooks(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = (
        self._dcommit_calls_1() +
        self._dcommit_calls_bypassed() +
        self._dcommit_calls_3())
    git_cl.main(['dcommit', '--bypass-hooks'])


  @classmethod
  def _gerrit_ensure_auth_calls(cls, issue=None, skip_auth_check=False):
    cmd = ['git', 'config', '--bool', 'gerrit.skip-ensure-authenticated']
    if skip_auth_check:
      return [((cmd, ), 'true')]

    calls = [((cmd, ), '', subprocess2.CalledProcessError(1, '', '', '', ''))]
    if issue:
      calls.extend([
          ((['git', 'config', 'branch.master.gerritserver'],), ''),
      ])
    calls.extend([
        ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.master.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/my/repo'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/my/repo'),
    ])
    return calls

  @classmethod
  def _gerrit_base_calls(cls, issue=None):
    return [
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
        ((['git', 'config', '--int', '--get',
          'branch.master.git-cl-similarity'],), ''),
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
        ((['git', 'config', '--int', '--get',
          'branch.master.git-find-copies'],), ''),
      ] + cls._is_gerrit_calls(True) + [
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
        ((['git', 'config', 'branch.master.rietveldissue'],), ''),
        ((['git', 'config', 'branch.master.gerritissue'],),
          '' if issue is None else str(issue)),
        ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.master.remote'],), 'origin'),
        ((['get_or_create_merge_base', 'master',
           'refs/remotes/origin/master'],),
         'fake_ancestor_sha'),
        # Calls to verify branch point is ancestor
      ] + (cls._gerrit_ensure_auth_calls(issue=issue) +
           cls._git_sanity_checks('fake_ancestor_sha', 'master',
                                  get_remote_branch=False)) + [
        ((['git', 'rev-parse', '--show-cdup'],), ''),
        ((['git', 'rev-parse', 'HEAD'],), '12345'),

        ((['git',
           'diff', '--name-status', '--no-renames', '-r',
           'fake_ancestor_sha...', '.'],),
         'M\t.gitignore\n'),
        ((['git', 'config', 'branch.master.gerritpatchset'],), ''),
      ] + ([] if issue else [
        ((['git',
           'log', '--pretty=format:%s%n%n%b', 'fake_ancestor_sha...'],),
         'foo'),
      ]) + [
        ((['git', 'config', 'user.email'],), 'me@example.com'),
        ((['git',
           'diff', '--no-ext-diff', '--stat', '--find-copies-harder',
           '-l100000', '-C50', 'fake_ancestor_sha', 'HEAD'],),
         '+dat'),
      ]

  @classmethod
  def _gerrit_upload_calls(cls, description, reviewers, squash,
                           squash_mode='default',
                           expected_upstream_ref='origin/refs/heads/master',
                           ref_suffix='', notify=False,
                           post_amend_description=None, issue=None):
    if post_amend_description is None:
      post_amend_description = description
    calls = []

    if squash_mode == 'default':
      calls.extend([
        ((['git', 'config', '--bool', 'gerrit.override-squash-uploads'],), ''),
        ((['git', 'config', '--bool', 'gerrit.squash-uploads'],), ''),
      ])
    elif squash_mode in ('override_squash', 'override_nosquash'):
      calls.extend([
        ((['git', 'config', '--bool', 'gerrit.override-squash-uploads'],),
         'true' if squash_mode == 'override_squash' else 'false'),
      ])
    else:
      assert squash_mode in ('squash', 'nosquash')

    # If issue is given, then description is fetched from Gerrit instead.
    if issue is None:
      calls += [
          ((['git', 'log', '--pretty=format:%s\n\n%b',
                   'fake_ancestor_sha..HEAD'],),
                  description)]
    if not git_footers.get_footer_change_id(description) and not squash:
      calls += [
          # DownloadGerritHook(False)
          ((False, ),
            ''),
          # Amending of commit message to get the Change-Id.
          ((['git', 'log', '--pretty=format:%s\n\n%b',
             'fake_ancestor_sha..HEAD'],),
           description),
          ((['git', 'commit', '--amend', '-m', description],),
           ''),
          ((['git', 'log', '--pretty=format:%s\n\n%b',
             'fake_ancestor_sha..HEAD'],),
           post_amend_description)
          ]
    if squash:
      if not issue:
        # Prompting to edit description on first upload.
        calls += [
            ((['git', 'config', 'core.editor'],), ''),
            ((['RunEditor'],), description),
        ]
      ref_to_push = 'abcdef0123456789'
      calls += [
          ((['git', 'config', 'branch.master.merge'],),
           'refs/heads/master'),
          ((['git', 'config', 'branch.master.remote'],),
           'origin'),
          ((['get_or_create_merge_base', 'master',
             'refs/remotes/origin/master'],),
           'origin/master'),
          ((['git', 'rev-parse', 'HEAD:'],),
           '0123456789abcdef'),
          ((['git', 'commit-tree', '0123456789abcdef', '-p',
             'origin/master', '-m', description],),
           ref_to_push),
          ]
    else:
      ref_to_push = 'HEAD'

    calls += [
        ((['git', 'rev-list',
            expected_upstream_ref + '..' + ref_to_push],), ''),
        ((['git', 'config', 'rietveld.cc'],), '')
        ]

    notify_suffix = 'notify=%s' % ('ALL' if notify else 'NONE')
    if ref_suffix:
      ref_suffix += ',' + notify_suffix
    else:
      ref_suffix = '%' + notify_suffix

    # Add cc from watch list.
    ref_suffix += ',cc=joe@example.com'

    if reviewers:
      ref_suffix += ',' + ','.join('r=%s' % email
                                   for email in sorted(reviewers))
    calls += [
        ((['git', 'push', 'origin',
           ref_to_push + ':refs/for/refs/heads/master' + ref_suffix],),
         ('remote:\n'
         'remote: Processing changes: (\)\n'
         'remote: Processing changes: (|)\n'
         'remote: Processing changes: (/)\n'
         'remote: Processing changes: (-)\n'
         'remote: Processing changes: new: 1 (/)\n'
         'remote: Processing changes: new: 1, done\n'
         'remote:\n'
         'remote: New Changes:\n'
         'remote:   https://chromium-review.googlesource.com/123456 XXX.\n'
         'remote:\n'
         'To https://chromium.googlesource.com/yyy/zzz\n'
         ' * [new branch]      hhhh -> refs/for/refs/heads/master\n')),
        ]
    if squash:
      calls += [
          ((['git', 'config', 'branch.master.gerritissue', '123456'],), ''),
          ((['git', 'config', 'branch.master.gerritserver',
          'https://chromium-review.googlesource.com'],), ''),
          ((['git', 'config', 'branch.master.gerritsquashhash',
             'abcdef0123456789'],), ''),
      ]
    calls += cls._git_post_upload_calls()
    return calls

  def _run_gerrit_upload_test(
      self,
      upload_args,
      description,
      reviewers=None,
      squash=True,
      squash_mode=None,
      expected_upstream_ref='origin/refs/heads/master',
      ref_suffix='',
      notify=False,
      post_amend_description=None,
      issue=None):
    """Generic gerrit upload test framework."""
    if squash_mode is None:
      if '--no-squash' in upload_args:
        squash_mode = 'nosquash'
      elif '--squash' in upload_args:
        squash_mode = 'squash'
      else:
        squash_mode = 'default'

    reviewers = reviewers or []
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.gerrit_util, 'CookiesAuthenticator',
              CookiesAuthenticatorMockFactory(same_cookie='same_cred'))
    self.mock(git_cl._GerritChangelistImpl, '_GerritCommitMsgHookCheck',
              lambda _, offer_removal: None)
    self.mock(git_cl.gclient_utils, 'RunEditor',
              lambda *_, **__: self._mocked_call(['RunEditor']))
    self.mock(git_cl, 'DownloadGerritHook', self._mocked_call)

    self.calls = self._gerrit_base_calls(issue=issue)
    self.calls += self._gerrit_upload_calls(
        description, reviewers, squash,
        squash_mode=squash_mode,
        expected_upstream_ref=expected_upstream_ref,
        ref_suffix=ref_suffix, notify=notify,
        post_amend_description=post_amend_description,
        issue=issue)
    # Uncomment when debugging.
    # print '\n'.join(map(lambda x: '%2i: %s' % x, enumerate(self.calls)))
    git_cl.main(['upload'] + upload_args)

  def test_gerrit_upload_without_change_id(self):
    self._run_gerrit_upload_test(
        ['--no-squash'],
        'desc\n\nBUG=\n',
        [],
        squash=False,
        post_amend_description='desc\n\nBUG=\n\nChange-Id: Ixxx')

  def test_gerrit_upload_without_change_id_override_nosquash(self):
    self.mock(git_cl, 'DownloadGerritHook', self._mocked_call)
    self._run_gerrit_upload_test(
        [],
        'desc\n\nBUG=\n',
        [],
        squash=False,
        squash_mode='override_nosquash',
        post_amend_description='desc\n\nBUG=\n\nChange-Id: Ixxx')

  def test_gerrit_no_reviewer(self):
    self._run_gerrit_upload_test(
        [],
        'desc\n\nBUG=\n\nChange-Id: I123456789\n',
        [],
        squash=False,
        squash_mode='override_nosquash')

  def test_gerrit_patch_title(self):
    self._run_gerrit_upload_test(
        ['-t', 'Don\'t put under_scores as they become spaces'],
        'desc\n\nBUG=\n\nChange-Id: I123456789',
        squash=False,
        squash_mode='override_nosquash',
        ref_suffix='%m=Don\'t_put_under_scores_as_they_become_spaces')

  def test_gerrit_reviewers_cmd_line(self):
    self._run_gerrit_upload_test(
        ['-r', 'foo@example.com', '--send-mail'],
        'desc\n\nBUG=\n\nChange-Id: I123456789',
        ['foo@example.com'],
        squash=False,
        squash_mode='override_nosquash',
        notify=True)

  def test_gerrit_reviewer_multiple(self):
    self._run_gerrit_upload_test(
        [],
        'desc\nTBR=reviewer@example.com\nBUG=\nR=another@example.com\n\n'
        'Change-Id: 123456789\n',
        ['reviewer@example.com', 'another@example.com'],
        squash=False,
        squash_mode='override_nosquash')

  def test_gerrit_upload_squash_first_is_default(self):
    # Mock Gerrit CL description to indicate the first upload.
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *_: None)
    self._run_gerrit_upload_test(
        [],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        expected_upstream_ref='origin/master')

  def test_gerrit_upload_squash_first(self):
    # Mock Gerrit CL description to indicate the first upload.
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *_: None)
    self._run_gerrit_upload_test(
        ['--squash'],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        squash=True,
        expected_upstream_ref='origin/master')

  def test_gerrit_upload_squash_reupload(self):
    description = 'desc\nBUG=\n\nChange-Id: 123456789'
    # Mock Gerrit CL description to indicate re-upload.
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *args: description)
    self.mock(git_cl.Changelist, 'GetMostRecentPatchset',
              lambda *args: 1)
    self.mock(git_cl._GerritChangelistImpl, '_GetChangeDetail',
              lambda *args: {'change_id': '123456789'})
    self._run_gerrit_upload_test(
        ['--squash'],
        description,
        [],
        squash=True,
        expected_upstream_ref='origin/master',
        issue=123456)

  def test_upload_branch_deps(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    def mock_run_git(*args, **_kwargs):
      if args[0] == ['for-each-ref',
                       '--format=%(refname:short) %(upstream:short)',
                       'refs/heads']:
        # Create a local branch dependency tree that looks like this:
        # test1 -> test2 -> test3   -> test4 -> test5
        #                -> test3.1
        # test6 -> test0
        branch_deps = [
            'test2 test1',    # test1 -> test2
            'test3 test2',    # test2 -> test3
            'test3.1 test2',  # test2 -> test3.1
            'test4 test3',    # test3 -> test4
            'test5 test4',    # test4 -> test5
            'test6 test0',    # test0 -> test6
            'test7',          # test7
        ]
        return '\n'.join(branch_deps)
    self.mock(git_cl, 'RunGit', mock_run_git)

    class RecordCalls:
      times_called = 0
    record_calls = RecordCalls()
    def mock_CMDupload(*args, **_kwargs):
      record_calls.times_called += 1
      return 0
    self.mock(git_cl, 'CMDupload', mock_CMDupload)

    self.calls = [
        (('[Press enter to continue or ctrl-C to quit]',), ''),
      ]

    class MockChangelist():
      def __init__(self):
        pass
      def GetBranch(self):
        return 'test1'
      def GetIssue(self):
        return '123'
      def GetPatchset(self):
        return '1001'
      def IsGerrit(self):
        return False

    ret = git_cl.upload_branch_deps(MockChangelist(), [])
    # CMDupload should have been called 5 times because of 5 dependent branches.
    self.assertEquals(5, record_calls.times_called)
    self.assertEquals(0, ret)

  def test_gerrit_change_id(self):
    self.calls = [
        ((['git', 'write-tree'], ),
          'hashtree'),
        ((['git', 'rev-parse', 'HEAD~0'], ),
          'branch-parent'),
        ((['git', 'var', 'GIT_AUTHOR_IDENT'], ),
          'A B <a@b.org> 1456848326 +0100'),
        ((['git', 'var', 'GIT_COMMITTER_IDENT'], ),
          'C D <c@d.org> 1456858326 +0100'),
        ((['git', 'hash-object', '-t', 'commit', '--stdin'], ),
          'hashchange'),
    ]
    change_id = git_cl.GenerateGerritChangeId('line1\nline2\n')
    self.assertEqual(change_id, 'Ihashchange')

  def test_desecription_append_footer(self):
    for init_desc, footer_line, expected_desc in [
      # Use unique desc first lines for easy test failure identification.
      ('foo', 'R=one', 'foo\n\nR=one'),
      ('foo\n\nR=one', 'BUG=', 'foo\n\nR=one\nBUG='),
      ('foo\n\nR=one', 'Change-Id: Ixx', 'foo\n\nR=one\n\nChange-Id: Ixx'),
      ('foo\n\nChange-Id: Ixx', 'R=one', 'foo\n\nR=one\n\nChange-Id: Ixx'),
      ('foo\n\nR=one\n\nChange-Id: Ixx', 'TBR=two',
       'foo\n\nR=one\nTBR=two\n\nChange-Id: Ixx'),
      ('foo\n\nR=one\n\nChange-Id: Ixx', 'Foo-Bar: baz',
       'foo\n\nR=one\n\nChange-Id: Ixx\nFoo-Bar: baz'),
      ('foo\n\nChange-Id: Ixx', 'Foo-Bak: baz',
       'foo\n\nChange-Id: Ixx\nFoo-Bak: baz'),
      ('foo', 'Change-Id: Ixx', 'foo\n\nChange-Id: Ixx'),
    ]:
      desc = git_cl.ChangeDescription(init_desc)
      desc.append_footer(footer_line)
      self.assertEqual(desc.description, expected_desc)

  def test_update_reviewers(self):
    data = [
      ('foo', [], 'foo'),
      ('foo\nR=xx', [], 'foo\nR=xx'),
      ('foo\nTBR=xx', [], 'foo\nTBR=xx'),
      ('foo', ['a@c'], 'foo\n\nR=a@c'),
      ('foo\nR=xx', ['a@c'], 'foo\n\nR=a@c, xx'),
      ('foo\nTBR=xx', ['a@c'], 'foo\n\nR=a@c\nTBR=xx'),
      ('foo\nTBR=xx\nR=yy', ['a@c'], 'foo\n\nR=a@c, yy\nTBR=xx'),
      ('foo\nBUG=', ['a@c'], 'foo\nBUG=\nR=a@c'),
      ('foo\nR=xx\nTBR=yy\nR=bar', ['a@c'], 'foo\n\nR=a@c, xx, bar\nTBR=yy'),
      ('foo', ['a@c', 'b@c'], 'foo\n\nR=a@c, b@c'),
      ('foo\nBar\n\nR=\nBUG=', ['c@c'], 'foo\nBar\n\nR=c@c\nBUG='),
      ('foo\nBar\n\nR=\nBUG=\nR=', ['c@c'], 'foo\nBar\n\nR=c@c\nBUG='),
      # Same as the line before, but full of whitespaces.
      (
        'foo\nBar\n\n R = \n BUG = \n R = ', ['c@c'],
        'foo\nBar\n\nR=c@c\n BUG =',
      ),
      # Whitespaces aren't interpreted as new lines.
      ('foo BUG=allo R=joe ', ['c@c'], 'foo BUG=allo R=joe\n\nR=c@c'),
    ]
    expected = [i[2] for i in data]
    actual = []
    for orig, reviewers, _expected in data:
      obj = git_cl.ChangeDescription(orig)
      obj.update_reviewers(reviewers)
      actual.append(obj.description)
    self.assertEqual(expected, actual)

  def test_get_target_ref(self):
    # Check remote or remote branch not present.
    self.assertEqual(None, git_cl.GetTargetRef('origin', None, 'master', None))
    self.assertEqual(None, git_cl.GetTargetRef(None,
                                               'refs/remotes/origin/master',
                                               'master', None))

    # Check default target refs for branches.
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/master',
                                         None, None))
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/lkgr',
                                         None, None))
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/lkcr',
                                         None, None))
    self.assertEqual('refs/branch-heads/123',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/branch-heads/123',
                                         None, None))
    self.assertEqual('refs/diff/test',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/origin/refs/diff/test',
                                         None, None))
    self.assertEqual('refs/heads/chrome/m42',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/origin/chrome/m42',
                                         None, None))

    # Check target refs for user-specified target branch.
    for branch in ('branch-heads/123', 'remotes/branch-heads/123',
                   'refs/remotes/branch-heads/123'):
      self.assertEqual('refs/branch-heads/123',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/origin/master',
                                           branch, None))
    for branch in ('origin/master', 'remotes/origin/master',
                   'refs/remotes/origin/master'):
      self.assertEqual('refs/heads/master',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/branch-heads/123',
                                           branch, None))
    for branch in ('master', 'heads/master', 'refs/heads/master'):
      self.assertEqual('refs/heads/master',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/branch-heads/123',
                                           branch, None))

    # Check target refs for pending prefix.
    self.assertEqual('prefix/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/master',
                                         None, 'prefix/'))

  def test_patch_when_dirty(self):
    # Patch when local tree is dirty
    self.mock(git_common, 'is_dirty_git_tree', lambda x: True)
    self.assertNotEqual(git_cl.main(['patch', '123456']), 0)

  def test_diff_when_dirty(self):
    # Do 'git cl diff' when local tree is dirty
    self.mock(git_common, 'is_dirty_git_tree', lambda x: True)
    self.assertNotEqual(git_cl.main(['diff']), 0)

  def _patch_common(self, is_gerrit=False, force_codereview=False):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl._RietveldChangelistImpl, 'GetMostRecentPatchset',
              lambda x: '60001')
    self.mock(git_cl._RietveldChangelistImpl, 'GetPatchSetDiff',
              lambda *args: None)
    self.mock(git_cl._GerritChangelistImpl, '_GetChangeDetail',
              lambda *args: {
                'current_revision': '7777777777',
                'revisions': {
                  '1111111111': {
                    '_number': 1,
                    'fetch': {'http': {
                      'url': 'https://chromium.googlesource.com/my/repo',
                      'ref': 'refs/changes/56/123456/1',
                    }},
                  },
                  '7777777777': {
                    '_number': 7,
                    'fetch': {'http': {
                      'url': 'https://chromium.googlesource.com/my/repo',
                      'ref': 'refs/changes/56/123456/7',
                    }},
                  },
                },
              })
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *args: 'Description')
    self.mock(git_cl, 'IsGitVersionAtLeast', lambda *args: True)

    self.calls = self.calls or []
    if not force_codereview:
      # These calls detect codereview to use.
      self.calls += [
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
        ((['git', 'config', 'branch.master.rietveldissue'],), ''),
        ((['git', 'config', 'branch.master.gerritissue'],), ''),
        ((['git', 'config', 'rietveld.autoupdate'],), ''),
      ]

    if is_gerrit:
      if not force_codereview:
        self.calls += [
          ((['git', 'config', 'gerrit.host'],), 'true'),
        ]
    else:
      self.calls += [
        ((['git', 'config', 'gerrit.host'],), ''),
        ((['git', 'config', 'rietveld.server'],), 'codereview.example.com'),
        ((['git', 'rev-parse', '--show-cdup'],), ''),
        ((['sed', '-e', 's|^--- a/|--- |; s|^+++ b/|+++ |'],), ''),
      ]

  def _common_patch_successful(self):
    self._patch_common()
    self.calls += [
      ((['git', 'apply', '--index', '-p0', '--3way'],), ''),
      ((['git', 'commit', '-m',
         'Description\n\n' +
         'patch from issue 123456 at patchset 60001 ' +
         '(http://crrev.com/123456#ps60001)'],), ''),
      ((['git', 'config', 'branch.master.rietveldissue', '123456'],), ''),
      ((['git', 'config', 'branch.master.rietveldserver'],), ''),
      ((['git', 'config', 'branch.master.rietveldserver',
         'https://codereview.example.com'],), ''),
      ((['git', 'config', 'branch.master.rietveldpatchset', '60001'],), ''),
    ]

  def test_patch_successful(self):
    self._common_patch_successful()
    self.assertEqual(git_cl.main(['patch', '123456']), 0)

  def test_patch_successful_new_branch(self):
    self.calls = [ ((['git', 'new-branch', 'master'],), ''), ]
    self._common_patch_successful()
    self.assertEqual(git_cl.main(['patch', '-b', 'master', '123456']), 0)

  def test_patch_conflict(self):
    self._patch_common()
    self.calls += [
      ((['git', 'apply', '--index', '-p0', '--3way'],), '',
       subprocess2.CalledProcessError(1, '', '', '', '')),
    ]
    self.assertNotEqual(git_cl.main(['patch', '123456']), 0)

  def test_gerrit_patch_successful(self):
    self._patch_common(is_gerrit=True)
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],), ''),
      ((['git', 'config', 'branch.master.gerritserver'],), ''),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/my/repo'),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://chromium-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '7'],), ''),
    ]
    self.assertEqual(git_cl.main(['patch', '123456']), 0)

  def test_patch_force_codereview(self):
    self._patch_common(is_gerrit=True, force_codereview=True)
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],), ''),
      ((['git', 'config', 'branch.master.gerritserver'],), ''),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/my/repo'),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://chromium-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '7'],), ''),
    ]
    self.assertEqual(git_cl.main(['patch', '--gerrit', '123456']), 0)

  def test_gerrit_patch_url_successful(self):
    self._patch_common(is_gerrit=True)
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/1'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],), ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://chromium-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '1'],), ''),
    ]
    self.assertEqual(git_cl.main(
      ['patch', 'https://chromium-review.googlesource.com/#/c/123456/1']), 0)

  def test_gerrit_patch_conflict(self):
    self._patch_common(is_gerrit=True)
    self.mock(git_cl, 'DieWithError',
              lambda msg: self._mocked_call(['DieWithError', msg]))
    class SystemExitMock(Exception):
      pass
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/1'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],),
        '', subprocess2.CalledProcessError(1, '', '', '', '')),
      ((['DieWithError', 'git cherry-pick FETCH_HEAD" failed.\n'],),
        '', SystemExitMock()),
    ]
    with self.assertRaises(SystemExitMock):
      git_cl.main(['patch',
                   'https://chromium-review.googlesource.com/#/c/123456/1'])

  def _checkout_calls(self):
    return [
        ((['git', 'config', '--local', '--get-regexp',
           'branch\\..*\\.rietveldissue'], ),
           ('branch.retrying.rietveldissue 1111111111\n'
            'branch.some-fix.rietveldissue 2222222222\n')),
        ((['git', 'config', '--local', '--get-regexp',
           'branch\\..*\\.gerritissue'], ),
           ('branch.ger-branch.gerritissue 123456\n'
            'branch.gbranch654.gerritissue 654321\n')),
    ]

  def test_checkout_gerrit(self):
    """Tests git cl checkout <issue>."""
    self.calls = self._checkout_calls()
    self.calls += [((['git', 'checkout', 'ger-branch'], ), '')]
    self.assertEqual(0, git_cl.main(['checkout', '123456']))

  def test_checkout_rietveld(self):
    """Tests git cl checkout <issue>."""
    self.calls = self._checkout_calls()
    self.calls += [((['git', 'checkout', 'some-fix'], ), '')]
    self.assertEqual(0, git_cl.main(['checkout', '2222222222']))

  def test_checkout_not_found(self):
    """Tests git cl checkout <issue>."""
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = self._checkout_calls()
    self.assertEqual(1, git_cl.main(['checkout', '99999']))

  def test_checkout_no_branch_issues(self):
    """Tests git cl checkout <issue>."""
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = [
        ((['git', 'config', '--local', '--get-regexp',
           'branch\\..*\\.rietveldissue'], ), '',
         subprocess2.CalledProcessError(1, '', '', '', '')),
        ((['git', 'config', '--local', '--get-regexp',
           'branch\\..*\\.gerritissue'], ), '',
         subprocess2.CalledProcessError(1, '', '', '', '')),

    ]
    self.assertEqual(1, git_cl.main(['checkout', '99999']))

  def _test_gerrit_ensure_authenticated_common(self, auth,
                                               skip_auth_check=False):
    self.mock(git_cl.gerrit_util, 'CookiesAuthenticator',
              CookiesAuthenticatorMockFactory(hosts_with_creds=auth))
    self.mock(git_cl, 'DieWithError',
              lambda msg: self._mocked_call(['DieWithError', msg]))
    self.mock(git_cl, 'ask_for_data',
              lambda msg: self._mocked_call(['ask_for_data', msg]))
    self.calls = self._gerrit_ensure_auth_calls(skip_auth_check=skip_auth_check)
    cl = git_cl.Changelist(codereview='gerrit')
    cl.branch = 'master'
    cl.branchref = 'refs/heads/master'
    cl.lookedup_issue = True
    return cl

  def test_gerrit_ensure_authenticated_missing(self):
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com': 'git is ok, but gerrit one is missing',
    })
    self.calls.append(
        ((['DieWithError',
           'Credentials for the following hosts are required:\n'
           '  chromium-review.googlesource.com\n'
           'These are read from ~/.gitcookies (or legacy ~/.netrc)\n'
           'You can (re)generate your credentails by visiting '
           'https://chromium-review.googlesource.com/new-password'],), ''),)
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_conflict(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com': 'one',
      'chromium-review.googlesource.com': 'other',
    })
    self.calls.append(
        ((['ask_for_data', 'If you know what you are doing, '
				   								 'press Enter to continue, Ctrl+C to abort.'],), ''))
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_ok(self):
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com': 'same',
      'chromium-review.googlesource.com': 'same',
    })
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_skipped(self):
    cl = self._test_gerrit_ensure_authenticated_common(
        auth={}, skip_auth_check=True)
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_cmd_set_commit_rietveld(self):
    self.mock(git_cl._RietveldChangelistImpl, 'SetFlags',
              lambda _, v: self._mocked_call(['SetFlags', v]))
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.rietveldissue'],), '123'),
        ((['git', 'config', 'rietveld.autoupdate'],), ''),
        ((['git', 'config', 'rietveld.server'],), ''),
        ((['git', 'config', 'rietveld.server'],), ''),
        ((['git', 'config', 'branch.feature.rietveldserver'],),
         'https://codereview.chromium.org'),
        ((['SetFlags', {'commit': '1', 'cq_dry_run': '0'}], ), ''),
    ]
    self.assertEqual(0, git_cl.main(['set-commit']))

  def _cmd_set_commit_gerrit_common(self, vote):
    self.mock(git_cl.gerrit_util, 'SetReview',
              lambda h, i, labels: self._mocked_call(
                  ['SetReview', h, i, labels]))
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.rietveldissue'],), ''),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', 'branch.feature.gerritserver'],),
         'https://chromium-review.googlesource.com'),
        ((['SetReview', 'chromium-review.googlesource.com', 123,
           {'Commit-Queue': vote}],), ''),
    ]

  def test_cmd_set_commit_gerrit_clear(self):
    self._cmd_set_commit_gerrit_common(0)
    self.assertEqual(0, git_cl.main(['set-commit', '-c']))

  def test_cmd_set_commit_gerrit_dry(self):
    self._cmd_set_commit_gerrit_common(1)
    self.assertEqual(0, git_cl.main(['set-commit', '-d']))

  def test_cmd_set_commit_gerrit(self):
    self._cmd_set_commit_gerrit_common(2)
    self.assertEqual(0, git_cl.main(['set-commit']))

  def test_description_display(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)

    self.mock(git_cl, 'Changelist', ChangelistMock)
    ChangelistMock.desc = 'foo\n'

    self.assertEqual(0, git_cl.main(['description', '-d']))
    self.assertEqual('foo\n', out.getvalue())

  def test_description_rietveld(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.mock(git_cl.Changelist, 'GetDescription', lambda *args: 'foobar')

    self.assertEqual(0, git_cl.main([
        'description', 'https://code.review.org/123123', '-d', '--rietveld']))
    self.assertEqual('foobar\n', out.getvalue())

  def test_description_gerrit(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.mock(git_cl.Changelist, 'GetDescription', lambda *args: 'foobar')

    self.assertEqual(0, git_cl.main([
        'description', 'https://code.review.org/123123', '-d', '--gerrit']))
    self.assertEqual('foobar\n', out.getvalue())

  def test_description_set_raw(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)

    self.mock(git_cl, 'Changelist', ChangelistMock)
    self.mock(git_cl.sys, 'stdin', StringIO.StringIO('hihi'))

    self.assertEqual(0, git_cl.main(['description', '-n', 'hihi']))
    self.assertEqual('hihi', ChangelistMock.desc)

  def test_description_appends_bug_line(self):
    current_desc = 'Some.\n\nChange-Id: xxx'

    def RunEditor(desc, _, **kwargs):
      self.assertEquals(
          '# Enter a description of the change.\n'
          '# This will be displayed on the codereview site.\n'
          '# The first line will also be used as the subject of the review.\n'
          '#--------------------This line is 72 characters long'
          '--------------------\n'
          'Some.\n\nBUG=\n\nChange-Id: xxx',
          desc)
      # Simulate user changing something.
      return 'Some.\n\nBUG=123\n\nChange-Id: xxx'

    def UpdateDescriptionRemote(_, desc):
      self.assertEquals(desc, 'Some.\n\nBUG=123\n\nChange-Id: xxx')

    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *args: current_desc)
    self.mock(git_cl._GerritChangelistImpl, 'UpdateDescriptionRemote',
              UpdateDescriptionRemote)
    self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)

    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', 'rietveld.autoupdate'],), ''),
        ((['git', 'config', 'rietveld.bug-prefix'],), ''),
        ((['git', 'config', 'core.editor'],), 'vi'),
    ]
    self.assertEqual(0, git_cl.main(['description', '--gerrit']))

  def test_description_set_stdin(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)

    self.mock(git_cl, 'Changelist', ChangelistMock)
    self.mock(git_cl.sys, 'stdin', StringIO.StringIO('hi \r\n\t there\n\nman'))

    self.assertEqual(0, git_cl.main(['description', '-n', '-']))
    self.assertEqual('hi\n\t there\n\nman', ChangelistMock.desc)

  def test_archive(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())

    self.calls = \
        [((['git', 'for-each-ref', '--format=%(refname)', 'refs/heads'],),
          'refs/heads/master\nrefs/heads/foo\nrefs/heads/bar'),
         ((['git', 'config', 'branch.master.rietveldissue'],), '1'),
         ((['git', 'config', 'rietveld.autoupdate'],), ''),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'config', 'branch.foo.rietveldissue'],), '456'),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'config', 'branch.bar.rietveldissue'],), ''),
         ((['git', 'config', 'branch.bar.gerritissue'],), '789'),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
         ((['git', 'tag', 'git-cl-archived-456-foo', 'foo'],), ''),
         ((['git', 'branch', '-D', 'foo'],), '')]

    class MockChangelist():
      def __init__(self, branch, issue):
        self.branch = branch
        self.issue = issue
      def GetBranch(self):
        return self.branch
      def GetIssue(self):
        return self.issue

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelist('master', 1), 'open'),
               (MockChangelist('foo', 456), 'closed'),
               (MockChangelist('bar', 789), 'open')])

    self.assertEqual(0, git_cl.main(['archive', '-f']))

  def test_archive_current_branch_fails(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = \
        [((['git', 'for-each-ref', '--format=%(refname)', 'refs/heads'],),
          'refs/heads/master'),
         ((['git', 'config', 'branch.master.rietveldissue'],), '1'),
         ((['git', 'config', 'rietveld.autoupdate'],), ''),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'config', 'rietveld.server'],), ''),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master')]

    class MockChangelist():
      def __init__(self, branch, issue):
        self.branch = branch
        self.issue = issue
      def GetBranch(self):
        return self.branch
      def GetIssue(self):
        return self.issue

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelist('master', 1), 'closed')])

    self.assertEqual(1, git_cl.main(['archive', '-f']))

  def test_cmd_issue_erase_existing(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.rietveldissue'],), ''),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', '--unset', 'branch.feature.gerritissue'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritpatchset'],), ''),
        # Let this command raise exception (retcode=1) - it should be ignored.
        ((['git', 'config', '--unset', 'branch.feature.last-upload-hash'],),
         '', subprocess2.CalledProcessError(1, '', '', '', '')),
        ((['git', 'config', '--unset', 'branch.feature.gerritserver'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritsquashhash'],),
         ''),
    ]
    self.assertEqual(0, git_cl.main(['issue', '0']))

  def test_git_cl_try_default(self):
    self.mock(git_cl.Changelist, 'GetChange',
              lambda _, *a: (
                self._mocked_call(['GetChange']+list(a))))
    self.mock(git_cl.presubmit_support, 'DoGetTryMasters',
              lambda *_, **__: (
                self._mocked_call(['DoGetTryMasters'])))
    self.mock(git_cl.presubmit_support, 'DoGetTrySlaves',
              lambda *_, **__: (
                self._mocked_call(['DoGetTrySlaves'])))
    self.mock(git_cl._RietveldChangelistImpl, 'SetCQState',
              lambda _, s: self._mocked_call(['SetCQState', s]))
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.rietveldissue'],), '123'),
        ((['git', 'config', 'rietveld.autoupdate'],), ''),
        ((['git', 'config', 'rietveld.server'],),
         'https://codereview.chromium.org'),
        ((['git', 'config', 'branch.feature.rietveldserver'],), ''),
        ((['git', 'config', 'branch.feature.merge'],), 'feature'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['get_or_create_merge_base', 'feature', 'feature'],),
         'fake_ancestor_sha'),
        ((['GetChange', 'fake_ancestor_sha', None], ),
         git_cl.presubmit_support.GitChange(
           '', '', '', '', '', '', '', '')),
        ((['git', 'rev-parse', '--show-cdup'],), '../'),
        ((['DoGetTryMasters'], ), None),
        ((['DoGetTrySlaves'], ), None),
        ((['SetCQState', git_cl._CQState.DRY_RUN], ), None),
    ]
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.assertEqual(0, git_cl.main(['try']))
    self.assertEqual(
        out.getvalue(),
        'scheduled CQ Dry Run on https://codereview.chromium.org/123\n')

  def _common_GerritCommitMsgHookCheck(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.os.path, 'abspath',
              lambda path: self._mocked_call(['abspath', path]))
    self.mock(git_cl.os.path, 'exists',
              lambda path: self._mocked_call(['exists', path]))
    self.mock(git_cl.gclient_utils, 'FileRead',
              lambda path: self._mocked_call(['FileRead', path]))
    self.mock(git_cl.gclient_utils, 'rm_file_or_tree',
              lambda path: self._mocked_call(['rm_file_or_tree', path]))
    self.calls = [
        ((['git', 'rev-parse', '--show-cdup'],), '../'),
        ((['abspath', '../'],), '/abs/git_repo_root'),
    ]
    return git_cl.Changelist(codereview='gerrit', issue=123)

  def test_GerritCommitMsgHookCheck_custom_hook(self):
    cl = self._common_GerritCommitMsgHookCheck()
    self.calls += [
        ((['exists', '/abs/git_repo_root/.git/hooks/commit-msg'],), True),
        ((['FileRead', '/abs/git_repo_root/.git/hooks/commit-msg'],),
         '#!/bin/sh\necho "custom hook"')
    ]
    cl._codereview_impl._GerritCommitMsgHookCheck(offer_removal=True)

  def test_GerritCommitMsgHookCheck_not_exists(self):
    cl = self._common_GerritCommitMsgHookCheck()
    self.calls += [
        ((['exists', '/abs/git_repo_root/.git/hooks/commit-msg'],), False),
    ]
    cl._codereview_impl._GerritCommitMsgHookCheck(offer_removal=True)

  def test_GerritCommitMsgHookCheck(self):
    cl = self._common_GerritCommitMsgHookCheck()
    self.calls += [
        ((['exists', '/abs/git_repo_root/.git/hooks/commit-msg'],), True),
        ((['FileRead', '/abs/git_repo_root/.git/hooks/commit-msg'],),
         '...\n# From Gerrit Code Review\n...\nadd_ChangeId()\n'),
        (('Do you want to remove it now? [Yes/No]',), 'Yes'),
        ((['rm_file_or_tree', '/abs/git_repo_root/.git/hooks/commit-msg'],),
         ''),
    ]
    cl._codereview_impl._GerritCommitMsgHookCheck(offer_removal=True)


if __name__ == '__main__':
  git_cl.logging.basicConfig(
      level=git_cl.logging.DEBUG if '-v' in sys.argv else git_cl.logging.ERROR)
  unittest.main()
