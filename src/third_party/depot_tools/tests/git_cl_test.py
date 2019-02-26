#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_cl.py."""

import contextlib
import datetime
import json
import logging
import os
import StringIO
import sys
import tempfile
import unittest
import urlparse

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from testing_support.auto_stub import TestCase

import metrics
# We have to disable monitoring before importing git_cl.
metrics.DISABLE_METRICS_COLLECTION = True

import gerrit_util
import git_cl
import git_common
import git_footers
import subprocess2

def callError(code=1, cmd='', cwd='', stdout='', stderr=''):
  return subprocess2.CalledProcessError(code, cmd, cwd, stdout, stderr)


CERR1 = callError(1)


def MakeNamedTemporaryFileMock(expected_content):
  class NamedTemporaryFileMock(object):
    def __init__(self, *args, **kwargs):
      self.name = '/tmp/named'
      self.expected_content = expected_content

    def __enter__(self):
      return self

    def __exit__(self, _type, _value, _tb):
      pass

    def write(self, content):
      if self.expected_content:
        assert content == self.expected_content

    def close(self):
      pass

  return NamedTemporaryFileMock


class ChangelistMock(object):
  # A class variable so we can access it when we don't have access to the
  # instance that's being set.
  desc = ""
  def __init__(self, **kwargs):
    pass
  def GetIssue(self):
    return 1
  def GetDescription(self, force=False):
    return ChangelistMock.desc
  def UpdateDescription(self, desc, force=False):
    ChangelistMock.desc = desc


class PresubmitMock(object):
  def __init__(self, *args, **kwargs):
    self.reviewers = []
    self.more_cc = ['chromium-reviews+test-more-cc@chromium.org']

  @staticmethod
  def should_continue():
    return True


class GitCheckoutMock(object):
  def __init__(self, *args, **kwargs):
    pass

  @staticmethod
  def reset():
    GitCheckoutMock.conflict = False

  def apply_patch(self, p):
    if GitCheckoutMock.conflict:
      raise Exception('failed')


class WatchlistsMock(object):
  def __init__(self, _):
    pass
  @staticmethod
  def GetWatchersForPaths(_):
    return ['joe@example.com']


class CodereviewSettingsFileMock(object):
  def __init__(self):
    pass
  # pylint: disable=no-self-use
  def read(self):
    return ("CODE_REVIEW_SERVER: gerrit.chromium.org\n" +
            "GERRIT_HOST: True\n")


class AuthenticatorMock(object):
  def __init__(self, *_args):
    pass
  def has_cached_credentials(self):
    return True
  def authorize(self, http):
    return http


def CookiesAuthenticatorMockFactory(hosts_with_creds=None, same_auth=False):
  """Use to mock Gerrit/Git credentials from ~/.netrc or ~/.gitcookies.

  Usage:
    >>> self.mock(git_cl.gerrit_util, "CookiesAuthenticator",
                  CookiesAuthenticatorMockFactory({'host': ('user', _, 'pass')})

  OR
    >>> self.mock(git_cl.gerrit_util, "CookiesAuthenticator",
                  CookiesAuthenticatorMockFactory(
                      same_auth=('user', '', 'pass'))
  """
  class CookiesAuthenticatorMock(git_cl.gerrit_util.CookiesAuthenticator):
    def __init__(self):  # pylint: disable=super-init-not-called
      # Intentionally not calling super() because it reads actual cookie files.
      pass
    @classmethod
    def get_gitcookies_path(cls):
      return '~/.gitcookies'
    @classmethod
    def get_netrc_path(cls):
      return '~/.netrc'
    def _get_auth_for_host(self, host):
      if same_auth:
        return same_auth
      return (hosts_with_creds or {}).get(host)
  return CookiesAuthenticatorMock


class MockChangelistWithBranchAndIssue():
  def __init__(self, branch, issue):
    self.branch = branch
    self.issue = issue
  def GetBranch(self):
    return self.branch
  def GetIssue(self):
    return self.issue


class SystemExitMock(Exception):
  pass


class TestGitClBasic(unittest.TestCase):
  def test_get_description(self):
    cl = git_cl.Changelist(issue=1, codereview='gerrit',
                           codereview_host='host')
    cl.description = 'x'
    cl.has_description = True
    cl._codereview_impl.FetchDescription = lambda *a, **kw: 'y'
    self.assertEquals(cl.GetDescription(), 'x')
    self.assertEquals(cl.GetDescription(force=True), 'y')
    self.assertEquals(cl.GetDescription(), 'y')

  def test_description_footers(self):
    cl = git_cl.Changelist(issue=1, codereview='gerrit',
                           codereview_host='host')
    cl.description = '\n'.join([
      'This is some message',
      '',
      'It has some lines',
      'and, also',
      '',
      'Some: Really',
      'Awesome: Footers',
    ])
    cl.has_description = True
    cl._codereview_impl.UpdateDescriptionRemote = lambda *a, **kw: 'y'
    msg, footers = cl.GetDescriptionFooters()
    self.assertEquals(
      msg, ['This is some message', '', 'It has some lines', 'and, also'])
    self.assertEquals(footers, [('Some', 'Really'), ('Awesome', 'Footers')])

    msg.append('wut')
    footers.append(('gnarly-dude', 'beans'))
    cl.UpdateDescriptionFooters(msg, footers)
    self.assertEquals(cl.GetDescription().splitlines(), [
      'This is some message',
      '',
      'It has some lines',
      'and, also',
      'wut'
      '',
      'Some: Really',
      'Awesome: Footers',
      'Gnarly-Dude: beans',
    ])

  def test_get_bug_line_values(self):
    f = lambda p, bugs: list(git_cl._get_bug_line_values(p, bugs))
    self.assertEqual(f('', ''), [])
    self.assertEqual(f('', '123,v8:456'), ['123', 'v8:456'])
    self.assertEqual(f('v8', '456'), ['v8:456'])
    self.assertEqual(f('v8', 'chromium:123,456'), ['v8:456', 'chromium:123'])
    # Not nice, but not worth carying.
    self.assertEqual(f('v8', 'chromium:123,456,v8:123'),
                     ['v8:456', 'chromium:123', 'v8:123'])

  def _test_git_number(self, parent_msg, dest_ref, child_msg,
                       parent_hash='parenthash'):
    desc = git_cl.ChangeDescription(child_msg)
    desc.update_with_git_number_footers(parent_hash, parent_msg, dest_ref)
    return desc.description

  def assertEqualByLine(self, actual, expected):
    self.assertEqual(actual.splitlines(), expected.splitlines())

  def test_git_number_bad_parent(self):
    with self.assertRaises(ValueError):
      self._test_git_number('Parent', 'refs/heads/master', 'Child')

  def test_git_number_bad_parent_footer(self):
    with self.assertRaises(AssertionError):
      self._test_git_number(
          'Parent\n'
          '\n'
          'Cr-Commit-Position: wrong',
          'refs/heads/master', 'Child')

  def test_git_number_bad_lineage_ignored(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#1}\n'
        'Cr-Branched-From: mustBeReal40CharHash-branch@{#pos}',
        'refs/heads/master', 'Child')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#2}\n'
        'Cr-Branched-From: mustBeReal40CharHash-branch@{#pos}')

  def test_git_number_same_branch(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#12}',
        dest_ref='refs/heads/master',
        child_msg='Child')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#13}')

  def test_git_number_same_branch_mixed_footers(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#12}',
        dest_ref='refs/heads/master',
        child_msg='Child\n'
                  '\n'
                  'Broken-by: design\n'
                  'BUG=123')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Broken-by: design\n'
        'BUG=123\n'
        'Cr-Commit-Position: refs/heads/master@{#13}')

  def test_git_number_same_branch_with_originals(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#12}',
        dest_ref='refs/heads/master',
        child_msg='Child\n'
        '\n'
        'Some users are smart and insert their own footers\n'
        '\n'
        'Cr-Whatever: value\n'
        'Cr-Commit-Position: refs/copy/paste@{#22}')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Some users are smart and insert their own footers\n'
        '\n'
        'Cr-Original-Whatever: value\n'
        'Cr-Original-Commit-Position: refs/copy/paste@{#22}\n'
        'Cr-Commit-Position: refs/heads/master@{#13}')

  def test_git_number_new_branch(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#12}',
        dest_ref='refs/heads/branch',
        child_msg='Child')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Cr-Commit-Position: refs/heads/branch@{#1}\n'
        'Cr-Branched-From: parenthash-refs/heads/master@{#12}')

  def test_git_number_lineage(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/branch@{#1}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}',
        dest_ref='refs/heads/branch',
        child_msg='Child')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Cr-Commit-Position: refs/heads/branch@{#2}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}')

  def test_git_number_moooooooore_lineage(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/branch@{#5}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}',
        dest_ref='refs/heads/mooore',
        child_msg='Child')
    self.assertEqualByLine(
        actual,
        'Child\n'
        '\n'
        'Cr-Commit-Position: refs/heads/mooore@{#1}\n'
        'Cr-Branched-From: parenthash-refs/heads/branch@{#5}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}')

  def test_git_number_ever_moooooooore_lineage(self):
    self.maxDiff = 10000  # pylint: disable=attribute-defined-outside-init
    actual = self._test_git_number(
        'CQ commit on fresh new branch + numbering.\n'
        '\n'
        'NOTRY=True\n'
        'NOPRESUBMIT=True\n'
        'BUG=\n'
        '\n'
        'Review-Url: https://codereview.chromium.org/2577703003\n'
        'Cr-Commit-Position: refs/heads/gnumb-test/br@{#1}\n'
        'Cr-Branched-From: 0749ff9edc-refs/heads/gnumb-test/cq@{#4}\n'
        'Cr-Branched-From: 5c49df2da6-refs/heads/master@{#41618}',
        dest_ref='refs/heads/gnumb-test/cl',
        child_msg='git cl on fresh new branch + numbering.\n'
                  '\n'
                  'Review-Url: https://codereview.chromium.org/2575043003 .\n')
    self.assertEqualByLine(
        actual,
        'git cl on fresh new branch + numbering.\n'
        '\n'
        'Review-Url: https://codereview.chromium.org/2575043003 .\n'
        'Cr-Commit-Position: refs/heads/gnumb-test/cl@{#1}\n'
        'Cr-Branched-From: parenthash-refs/heads/gnumb-test/br@{#1}\n'
        'Cr-Branched-From: 0749ff9edc-refs/heads/gnumb-test/cq@{#4}\n'
        'Cr-Branched-From: 5c49df2da6-refs/heads/master@{#41618}')

  def test_git_number_cherry_pick(self):
    actual = self._test_git_number(
        'Parent\n'
        '\n'
        'Cr-Commit-Position: refs/heads/branch@{#1}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}',
        dest_ref='refs/heads/branch',
        child_msg='Child, which is cherry-pick from master\n'
        '\n'
        'Cr-Commit-Position: refs/heads/master@{#100}\n'
        '(cherry picked from commit deadbeef12345678deadbeef12345678deadbeef)')
    self.assertEqualByLine(
        actual,
        'Child, which is cherry-pick from master\n'
        '\n'
        '(cherry picked from commit deadbeef12345678deadbeef12345678deadbeef)\n'
        '\n'
        'Cr-Original-Commit-Position: refs/heads/master@{#100}\n'
        'Cr-Commit-Position: refs/heads/branch@{#2}\n'
        'Cr-Branched-From: somehash-refs/heads/master@{#12}')

  def test_gerrit_mirror_hack(self):
    cr = 'chromium-review.googlesource.com'
    url0 = 'https://%s/a/changes/x?a=b' % cr
    origMirrors = git_cl.gerrit_util._GERRIT_MIRROR_PREFIXES
    try:
      git_cl.gerrit_util._GERRIT_MIRROR_PREFIXES = ['us1', 'us2']
      url1 = git_cl.gerrit_util._UseGerritMirror(url0, cr)
      url2 = git_cl.gerrit_util._UseGerritMirror(url1, cr)
      url3 = git_cl.gerrit_util._UseGerritMirror(url2, cr)

      self.assertNotEqual(url1, url2)
      self.assertEqual(sorted((url1, url2)), [
        'https://us1-mirror-chromium-review.googlesource.com/a/changes/x?a=b',
        'https://us2-mirror-chromium-review.googlesource.com/a/changes/x?a=b'])
      self.assertEqual(url1, url3)
    finally:
      git_cl.gerrit_util._GERRIT_MIRROR_PREFIXES = origMirrors

  def test_valid_accounts(self):
    mock_per_account = {
      'u1': None,  # 404, doesn't exist.
      'u2': {
        '_account_id': 123124,
        'avatars': [],
        'email': 'u2@example.com',
        'name': 'User Number 2',
        'status': 'OOO',
      },
      'u3': git_cl.gerrit_util.GerritError(500, 'retries didn\'t help :('),
    }
    def GetAccountDetailsMock(_, account):
      # Poor-man's mock library's side_effect.
      v = mock_per_account.pop(account)
      if isinstance(v, Exception):
        raise v
      return v

    original = git_cl.gerrit_util.GetAccountDetails
    try:
      git_cl.gerrit_util.GetAccountDetails = GetAccountDetailsMock
      actual = git_cl.gerrit_util.ValidAccounts(
          'host', ['u1', 'u2', 'u3'], max_threads=1)
    finally:
      git_cl.gerrit_util.GetAccountDetails = original
    self.assertEqual(actual, {
      'u2': {
        '_account_id': 123124,
        'avatars': [],
        'email': 'u2@example.com',
        'name': 'User Number 2',
        'status': 'OOO',
      },
    })



class TestParseIssueURL(unittest.TestCase):
  def _validate(self, parsed, issue=None, patchset=None, hostname=None,
                codereview=None, fail=False):
    self.assertIsNotNone(parsed)
    if fail:
      self.assertFalse(parsed.valid)
      return
    self.assertTrue(parsed.valid)
    self.assertEqual(parsed.issue, issue)
    self.assertEqual(parsed.patchset, patchset)
    self.assertEqual(parsed.hostname, hostname)
    self.assertEqual(parsed.codereview, codereview)

  def _run_and_validate(self, func, url, *args, **kwargs):
    result = func(urlparse.urlparse(url))
    if kwargs.pop('fail', False):
      self.assertIsNone(result)
      return None
    self._validate(result, *args, fail=False, **kwargs)

  def test_gerrit(self):
    def test(url, issue=None, patchset=None, hostname=None, fail=None):
      self._test_ParseIssueUrl(
          git_cl._GerritChangelistImpl.ParseIssueURL,
          url, issue, patchset, hostname, fail)
    def test(url, *args, **kwargs):
      self._run_and_validate(git_cl._GerritChangelistImpl.ParseIssueURL, url,
                             *args, codereview='gerrit', **kwargs)

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
    def test(arg, *args, **kwargs):
      codereview_hint = kwargs.pop('hint', None)
      self._validate(git_cl.ParseIssueNumberArgument(arg, codereview_hint),
                     *args, **kwargs)

    test('123', 123)
    test('', fail=True)
    test('abc', fail=True)
    test('123/1', fail=True)
    test('123a', fail=True)
    test('ssh://chrome-review.source.com/#/c/123/4/', fail=True)

    # Looks like Rietveld and Gerrit, but we should select Gerrit now
    # w/ or w/o hint.
    test('https://codereview.source.com/123',
         123, None, 'codereview.source.com', 'gerrit',
         hint='gerrit')
    test('https://codereview.source.com/123',
         123, None, 'codereview.source.com', 'gerrit')

    # Gerrrit.
    test('https://chrome-review.source.com/c/123/4',
         123, 4, 'chrome-review.source.com', 'gerrit')
    test('https://chrome-review.source.com/bad/123/4', fail=True)


class GitCookiesCheckerTest(TestCase):
  def setUp(self):
    super(GitCookiesCheckerTest, self).setUp()
    self.c = git_cl._GitCookiesChecker()
    self.c._all_hosts = []

  def mock_hosts_creds(self, subhost_identity_pairs):
    def ensure_googlesource(h):
      if not h.endswith(self.c._GOOGLESOURCE):
        assert not h.endswith('.')
        return h + '.' + self.c._GOOGLESOURCE
      return h
    self.c._all_hosts = [(ensure_googlesource(h), i, '.gitcookies')
                         for h, i in subhost_identity_pairs]

  def test_identity_parsing(self):
    self.assertEqual(self.c._parse_identity('ldap.google.com'),
                     ('ldap', 'google.com'))
    self.assertEqual(self.c._parse_identity('git-ldap.example.com'),
                     ('ldap', 'example.com'))
    # Specical case because we know there are no subdomains in chromium.org.
    self.assertEqual(self.c._parse_identity('git-note.period.chromium.org'),
                     ('note.period', 'chromium.org'))
    # Pathological: ".period." can be either username OR domain, more likely
    # domain.
    self.assertEqual(self.c._parse_identity('git-note.period.example.com'),
                     ('note', 'period.example.com'))

  def test_analysis_nothing(self):
    self.c._all_hosts = []
    self.assertFalse(self.c.has_generic_host())
    self.assertEqual(set(), self.c.get_conflicting_hosts())
    self.assertEqual(set(), self.c.get_duplicated_hosts())
    self.assertEqual(set(), self.c.get_partially_configured_hosts())
    self.assertEqual(set(), self.c.get_hosts_with_wrong_identities())

  def test_analysis(self):
    self.mock_hosts_creds([
      ('.googlesource.com',      'git-example.chromium.org'),

      ('chromium',               'git-example.google.com'),
      ('chromium-review',        'git-example.google.com'),
      ('chrome-internal',        'git-example.chromium.org'),
      ('chrome-internal-review', 'git-example.chromium.org'),
      ('conflict',               'git-example.google.com'),
      ('conflict-review',        'git-example.chromium.org'),
      ('dup',                    'git-example.google.com'),
      ('dup',                    'git-example.google.com'),
      ('dup-review',             'git-example.google.com'),
      ('partial',                'git-example.google.com'),
      ('gpartial-review',        'git-example.google.com'),
    ])
    self.assertTrue(self.c.has_generic_host())
    self.assertEqual(set(['conflict.googlesource.com']),
                     self.c.get_conflicting_hosts())
    self.assertEqual(set(['dup.googlesource.com']),
                     self.c.get_duplicated_hosts())
    self.assertEqual(set(['partial.googlesource.com',
                          'gpartial-review.googlesource.com']),
                     self.c.get_partially_configured_hosts())
    self.assertEqual(set(['chromium.googlesource.com',
                          'chrome-internal.googlesource.com']),
                     self.c.get_hosts_with_wrong_identities())

  def test_report_no_problems(self):
    self.test_analysis_nothing()
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.assertFalse(self.c.find_and_report_problems())
    self.assertEqual(sys.stdout.getvalue(), '')

  def test_report(self):
    self.test_analysis()
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.gerrit_util.CookiesAuthenticator, 'get_gitcookies_path',
              classmethod(lambda _: '~/.gitcookies'))
    self.assertTrue(self.c.find_and_report_problems())
    with open(os.path.join(os.path.dirname(__file__),
                           'git_cl_creds_check_report.txt')) as f:
      expected = f.read()
    def by_line(text):
      return [l.rstrip() for l in text.rstrip().splitlines()]
    self.maxDiff = 10000  # pylint: disable=attribute-defined-outside-init
    self.assertEqual(by_line(sys.stdout.getvalue().strip()), by_line(expected))


class TestGitCl(TestCase):
  def setUp(self):
    super(TestGitCl, self).setUp()
    self.calls = []
    self._calls_done = []
    self.mock(git_cl, 'time_time',
              lambda: self._mocked_call('time.time'))
    self.mock(git_cl.metrics.collector, 'add_repeated',
              lambda *a: self._mocked_call('add_repeated', *a))
    self.mock(subprocess2, 'call', self._mocked_call)
    self.mock(subprocess2, 'check_call', self._mocked_call)
    self.mock(subprocess2, 'check_output', self._mocked_call)
    self.mock(subprocess2, 'communicate',
              lambda *a, **kw: ([self._mocked_call(*a, **kw), ''], 0))
    self.mock(git_cl.gclient_utils, 'CheckCallAndFilter', self._mocked_call)
    self.mock(git_common, 'is_dirty_git_tree', lambda x: False)
    self.mock(git_common, 'get_or_create_merge_base',
              lambda *a: (
                  self._mocked_call(['get_or_create_merge_base']+list(a))))
    self.mock(git_cl, 'BranchExists', lambda _: True)
    self.mock(git_cl, 'FindCodereviewSettingsFile', lambda: '')
    self.mock(git_cl, 'SaveDescriptionBackup', lambda _:
              self._mocked_call('SaveDescriptionBackup'))
    self.mock(git_cl, 'ask_for_data', lambda *a, **k: self._mocked_call(
              *(['ask_for_data'] + list(a)), **k))
    self.mock(git_cl, 'write_json', lambda path, contents:
              self._mocked_call('write_json', path, contents))
    self.mock(git_cl.presubmit_support, 'DoPresubmitChecks', PresubmitMock)
    self.mock(git_cl.checkout, 'GitCheckout', GitCheckoutMock)
    GitCheckoutMock.reset()
    self.mock(git_cl.watchlists, 'Watchlists', WatchlistsMock)
    self.mock(git_cl.auth, 'get_authenticator_for_host', AuthenticatorMock)
    self.mock(git_cl.gerrit_util, 'GetChangeDetail',
              lambda *args, **kwargs: self._mocked_call(
                  'GetChangeDetail', *args, **kwargs))
    self.mock(git_cl.gerrit_util, 'GetChangeComments',
              lambda *args, **kwargs: self._mocked_call(
                  'GetChangeComments', *args, **kwargs))
    self.mock(git_cl.gerrit_util, 'AddReviewers',
              lambda h, i, reviewers, ccs, notify: self._mocked_call(
                  'AddReviewers', h, i, reviewers, ccs, notify))
    self.mock(git_cl.gerrit_util, 'SetReview',
              lambda h, i, msg=None, labels=None, notify=None:
                  self._mocked_call('SetReview', h, i, msg, labels, notify))
    self.mock(git_cl.gerrit_util.LuciContextAuthenticator, 'is_luci',
              staticmethod(lambda: False))
    self.mock(git_cl.gerrit_util.GceAuthenticator, 'is_gce',
              classmethod(lambda _: False))
    self.mock(git_cl.gerrit_util, 'ValidAccounts',
              lambda host, accounts:
                  self._mocked_call('ValidAccounts', host, accounts))
    self.mock(git_cl, 'DieWithError',
              lambda msg, change=None: self._mocked_call(['DieWithError', msg]))
    # It's important to reset settings to not have inter-tests interference.
    git_cl.settings = None

  def tearDown(self):
    try:
      self.assertEquals([], self.calls)
    except AssertionError:
      if not self.has_failed():
        raise
      # Sadly, has_failed() returns True if this OR any other tests before this
      # one have failed.
      git_cl.logging.error(
          '!!!!!!  IF YOU SEE THIS, READ BELOW, IT WILL SAVE YOUR TIME  !!!!!\n'
          'There are un-consumed self.calls after this test has finished.\n'
          'If you don\'t know which test this is, run:\n'
          '   tests/git_cl_tests.py -v\n'
          'If you are already running only this test, then **first** fix the '
          'problem whose exception is emitted below by unittest runner.\n'
          'Else, to be sure what\'s going on, run this test **alone** with \n'
          '    tests/git_cl_tests.py TestGitCl.<name>\n'
          'and follow instructions above.\n' +
          '=' * 80)
    finally:
      super(TestGitCl, self).tearDown()

  def _mocked_call(self, *args, **_kwargs):
    self.assertTrue(
        self.calls,
        '@%d  Expected: <Missing>   Actual: %r' % (len(self._calls_done), args))
    top = self.calls.pop(0)
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

      self.fail('@%d\n'
                '  Expected: %r\n'
                '  Actual:   %r' % (
          len(self._calls_done), expected_args, args))

    self._calls_done.append(top)
    if isinstance(result, Exception):
      raise result
    return result

  def test_ask_for_explicit_yes_true(self):
    self.calls = [
        (('ask_for_data', 'prompt [Yes/No]: '), 'blah'),
        (('ask_for_data', 'Please, type yes or no: '), 'ye'),
    ]
    self.assertTrue(git_cl.ask_for_explicit_yes('prompt'))

  def test_LoadCodereviewSettingsFromFile_gerrit(self):
    codereview_file = StringIO.StringIO('GERRIT_HOST: true')
    self.calls = [
      ((['git', 'config', '--unset-all', 'rietveld.cc'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.private'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.tree-status-url'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.viewvc-url'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.bug-prefix'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.cpplint-regex'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.cpplint-ignore-regex'],),
        CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.project'],), CERR1),
      ((['git', 'config', '--unset-all', 'rietveld.run-post-upload-hook'],),
        CERR1),
      ((['git', 'config', 'gerrit.host', 'true'],), ''),
    ]
    self.assertIsNone(git_cl.LoadCodereviewSettingsFromFile(codereview_file))

  @classmethod
  def _is_gerrit_calls(cls, gerrit=False):
    return [((['git', 'config', 'rietveld.autoupdate'],), ''),
            ((['git', 'config', 'gerrit.host'],), 'True' if gerrit else '')]

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
         'config', 'gitcl.remotebranch'],), CERR1),
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
  def _gerrit_ensure_auth_calls(
      cls, issue=None, skip_auth_check=False, short_hostname='chromium'):
    cmd = ['git', 'config', '--bool', 'gerrit.skip-ensure-authenticated']
    if skip_auth_check:
      return [((cmd, ), 'true')]

    calls = [((cmd, ), CERR1)]
    if issue:
      calls.extend([
          ((['git', 'config', 'branch.master.gerritserver'],), CERR1),
      ])
    calls.extend([
        ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.master.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://%s.googlesource.com/my/repo' % short_hostname),
    ])
    return calls

  @classmethod
  def _gerrit_base_calls(cls, issue=None, fetched_description=None,
                         fetched_status=None, other_cl_owner=None,
                         custom_cl_base=None, short_hostname='chromium'):
    calls = cls._is_gerrit_calls(True)
    calls += [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.gerritissue'],),
        CERR1 if issue is None else str(issue)),
    ]

    if custom_cl_base:
      ancestor_revision = custom_cl_base
    else:
      # Determine ancestor_revision to be merge base.
      ancestor_revision = 'fake_ancestor_sha'
      calls += [
        ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.master.remote'],), 'origin'),
        ((['get_or_create_merge_base', 'master',
           'refs/remotes/origin/master'],), ancestor_revision),
      ]

    # Calls to verify branch point is ancestor
    calls += cls._gerrit_ensure_auth_calls(
        issue=issue, short_hostname=short_hostname)

    if issue:
      calls += [
        (('GetChangeDetail', '%s-review.googlesource.com' % short_hostname,
          'my%2Frepo~123456',
          ['DETAILED_ACCOUNTS', 'CURRENT_REVISION', 'CURRENT_COMMIT', 'LABELS']
         ),
         {
           'owner': {'email': (other_cl_owner or 'owner@example.com')},
           'change_id': '123456789',
           'current_revision': 'sha1_of_current_revision',
           'revisions': { 'sha1_of_current_revision': {
             'commit': {'message': fetched_description},
           }},
           'status': fetched_status or 'NEW',
         }),
      ]
      if fetched_status == 'ABANDONED':
        calls += [
          (('DieWithError', 'Change https://%s-review.googlesource.com/'
                            '123456 has been abandoned, new uploads are not '
                            'allowed' % short_hostname), SystemExitMock()),
        ]
        return calls
      if other_cl_owner:
        calls += [
          (('ask_for_data', 'Press Enter to upload, or Ctrl+C to abort'), ''),
        ]

    calls += cls._git_sanity_checks(ancestor_revision, 'master',
                                    get_remote_branch=False)
    calls += [
      ((['git', 'rev-parse', '--show-cdup'],), ''),
      ((['git', 'rev-parse', 'HEAD'],), '12345'),

      ((['git', '-c', 'core.quotePath=false', 'diff', '--name-status',
         '--no-renames', '-r', ancestor_revision + '...', '.'],),
       'M\t.gitignore\n'),
      ((['git', 'config', 'branch.master.gerritpatchset'],), CERR1),
    ]

    if not issue:
      calls += [
        ((['git', 'log', '--pretty=format:%s%n%n%b',
           ancestor_revision + '...'],),
         'foo'),
      ]

    calls += [
      ((['git', 'config', 'user.email'],), 'me@example.com'),
      ((['git', 'diff', '--no-ext-diff', '--stat', '-l100000', '-C50'] +
         ([custom_cl_base] if custom_cl_base else
          [ancestor_revision, 'HEAD']),),
       '+dat'),
    ]
    return calls

  @classmethod
  def _gerrit_upload_calls(cls, description, reviewers, squash,
                           squash_mode='default',
                           expected_upstream_ref='origin/refs/heads/master',
                           title=None, notify=False,
                           post_amend_description=None, issue=None, cc=None,
                           custom_cl_base=None, tbr=None,
                           short_hostname='chromium',
                           labels=None):
    if post_amend_description is None:
      post_amend_description = description
    cc = cc or []
    # Determined in `_gerrit_base_calls`.
    determined_ancestor_revision = custom_cl_base or 'fake_ancestor_sha'

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
           ((custom_cl_base + '..') if custom_cl_base else
            'fake_ancestor_sha..HEAD')],),
         description),
      ]
      if squash:
        title = 'Initial_upload'
    else:
      if not title:
        calls += [
          ((['git', 'show', '-s', '--format=%s', 'HEAD'],), ''),
          (('ask_for_data', 'Title for patchset []: '), 'User input'),
        ]
        title = 'User_input'
    if not git_footers.get_footer_change_id(description) and not squash:
      calls += [
        (('DownloadGerritHook', False), ''),
        # Amending of commit message to get the Change-Id.
        ((['git', 'log', '--pretty=format:%s\n\n%b',
           determined_ancestor_revision + '..HEAD'],),
         description),
        ((['git', 'commit', '--amend', '-m', description],), ''),
        ((['git', 'log', '--pretty=format:%s\n\n%b',
           determined_ancestor_revision + '..HEAD'],),
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
        ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ]

      if custom_cl_base is None:
        calls += [
          ((['get_or_create_merge_base', 'master',
             'refs/remotes/origin/master'],),
           'origin/master'),
        ]
        parent = 'origin/master'
      else:
        calls += [
          ((['git', 'merge-base', '--is-ancestor', custom_cl_base,
             'refs/remotes/origin/master'],),
           callError(1)),   # Means not ancenstor.
          (('ask_for_data',
            'Do you take responsibility for cleaning up potential mess '
            'resulting from proceeding with upload? Press Enter to upload, '
            'or Ctrl+C to abort'), ''),
        ]
        parent = custom_cl_base

      calls += [
        ((['git', 'rev-parse', 'HEAD:'],),  # `HEAD:` means HEAD's tree hash.
         '0123456789abcdef'),
        ((['git', 'commit-tree', '0123456789abcdef', '-p', parent,
           '-F', '/tmp/named'],),
         ref_to_push),
      ]
    else:
      ref_to_push = 'HEAD'

    calls += [
      (('SaveDescriptionBackup',), None),
      ((['git', 'rev-list',
         (custom_cl_base if custom_cl_base else expected_upstream_ref) + '..' +
         ref_to_push],),
      '1hashPerLine\n'),
    ]

    metrics_arguments = []

    if notify:
      ref_suffix = '%ready,notify=ALL'
      metrics_arguments += ['ready', 'notify=ALL']
    else:
      if not issue and squash:
        ref_suffix = '%wip'
        metrics_arguments.append('wip')
      else:
        ref_suffix = '%notify=NONE'
        metrics_arguments.append('notify=NONE')

    if title:
      ref_suffix += ',m=' + title
      metrics_arguments.append('m')

    calls += [
      ((['git', 'config', 'rietveld.cc'],), ''),
    ]
    if short_hostname == 'chromium':
      # All reviwers and ccs get into ref_suffix.
      for r in sorted(reviewers):
        ref_suffix += ',r=%s' % r
        metrics_arguments.append('r')
      for c in sorted(['chromium-reviews+test-more-cc@chromium.org',
                      'joe@example.com'] + cc):
        ref_suffix += ',cc=%s' % c
        metrics_arguments.append('cc')
      reviewers, cc = [], []
    else:
      # TODO(crbug/877717): remove this case.
      calls += [
        (('ValidAccounts', '%s-review.googlesource.com' % short_hostname,
          sorted(reviewers) + ['joe@example.com',
          'chromium-reviews+test-more-cc@chromium.org'] + cc),
         {
           e: {'email': e}
           for e in (reviewers + ['joe@example.com'] + cc)
         })
      ]
      for r in sorted(reviewers):
        if r != 'bad-account-or-email':
          ref_suffix  += ',r=%s' % r
          metrics_arguments.append('r')
          reviewers.remove(r)
      for c in sorted(['joe@example.com'] + cc):
        ref_suffix += ',cc=%s' % c
        metrics_arguments.append('cc')
        if c in cc:
          cc.remove(c)

    if not tbr:
      for k, v in sorted((labels or {}).items()):
        ref_suffix += ',l=%s+%d' % (k, v)
        metrics_arguments.append('l=%s+%d' % (k, v))

    calls += [
      (('time.time',), 1000,),
      ((['git', 'push',
         'https://%s.googlesource.com/my/repo' % short_hostname,
         ref_to_push + ':refs/for/refs/heads/master' + ref_suffix],),
       (('remote:\n'
         'remote: Processing changes: (\)\n'
         'remote: Processing changes: (|)\n'
         'remote: Processing changes: (/)\n'
         'remote: Processing changes: (-)\n'
         'remote: Processing changes: new: 1 (/)\n'
         'remote: Processing changes: new: 1, done\n'
         'remote:\n'
         'remote: New Changes:\n'
         'remote:   https://%s-review.googlesource.com/#/c/my/repo/+/123456'
             ' XXX\n'
         'remote:\n'
         'To https://%s.googlesource.com/my/repo\n'
         ' * [new branch]      hhhh -> refs/for/refs/heads/master\n'
         ) % (short_hostname, short_hostname)),),
      (('time.time',), 2000,),
      (('add_repeated',
        'sub_commands',
        {
          'execution_time': 1000,
          'command': 'git push',
          'exit_code': 0,
          'arguments': sorted(metrics_arguments),
        }),
        None,),
    ]

    if squash:
      calls += [
          ((['git', 'config', 'branch.master.gerritissue', '123456'],),
           ''),
          ((['git', 'config', 'branch.master.gerritserver',
             'https://chromium-review.googlesource.com'],), ''),
          ((['git', 'config', 'branch.master.gerritsquashhash',
             'abcdef0123456789'],), ''),
      ]
    # TODO(crbug/877717): this should never be used.
    if squash and short_hostname != 'chromium':
      calls += [
          (('AddReviewers',
            'chromium-review.googlesource.com', 'my%2Frepo~123456',
            sorted(reviewers),
            cc + ['chromium-reviews+test-more-cc@chromium.org'],
            notify),
           ''),
      ]
    if tbr:
      calls += [
        (('GetChangeDetail', 'chromium-review.googlesource.com',
          'my%2Frepo~123456', ['LABELS']), {
             'labels': {
                 'Code-Review': {
                     'default_value': 0,
                     'all': [],
                     'values': {
                         '+2': 'lgtm, approved',
                         '+1': 'lgtm, but someone else must approve',
                         ' 0': 'No score',
                         '-1': 'Don\'t submit as-is',
                     }
                 }
              }
          }),
        (('SetReview',
          'chromium-review.googlesource.com',
          'my%2Frepo~123456',
          'Self-approving for TBR',
          {'Code-Review': 2}, None), ''),
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
      title=None,
      notify=False,
      post_amend_description=None,
      issue=None,
      cc=None,
      fetched_status=None,
      other_cl_owner=None,
      custom_cl_base=None,
      tbr=None,
      short_hostname='chromium',
      labels=None):
    """Generic gerrit upload test framework."""
    if squash_mode is None:
      if '--no-squash' in upload_args:
        squash_mode = 'nosquash'
      elif '--squash' in upload_args:
        squash_mode = 'squash'
      else:
        squash_mode = 'default'

    reviewers = reviewers or []
    cc = cc or []
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.gerrit_util, 'CookiesAuthenticator',
              CookiesAuthenticatorMockFactory(
                same_auth=('git-owner.example.com', '', 'pass')))
    self.mock(git_cl._GerritChangelistImpl, '_GerritCommitMsgHookCheck',
              lambda _, offer_removal: None)
    self.mock(git_cl.gclient_utils, 'RunEditor',
              lambda *_, **__: self._mocked_call(['RunEditor']))
    self.mock(git_cl, 'DownloadGerritHook', lambda force: self._mocked_call(
      'DownloadGerritHook', force))

    self.calls = self._gerrit_base_calls(
        issue=issue,
        fetched_description=description,
        fetched_status=fetched_status,
        other_cl_owner=other_cl_owner,
        custom_cl_base=custom_cl_base,
        short_hostname=short_hostname)
    if fetched_status != 'ABANDONED':
      self.mock(tempfile, 'NamedTemporaryFile', MakeNamedTemporaryFileMock(
          expected_content=description))
      self.mock(os, 'remove', lambda _: True)
      self.calls += self._gerrit_upload_calls(
          description, reviewers, squash,
          squash_mode=squash_mode,
          expected_upstream_ref=expected_upstream_ref,
          title=title, notify=notify,
          post_amend_description=post_amend_description,
          issue=issue, cc=cc,
          custom_cl_base=custom_cl_base, tbr=tbr,
          short_hostname=short_hostname,
          labels=labels)
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

  def test_gerrit_no_reviewer_non_chromium_host(self):
    # TODO(crbug/877717): remove this test case.
    self._run_gerrit_upload_test(
        [],
        'desc\n\nBUG=\n\nChange-Id: I123456789\n',
        [],
        squash=False,
        squash_mode='override_nosquash',
        short_hostname='other')

  def test_gerrit_patchset_title_special_chars(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self._run_gerrit_upload_test(
        ['-f', '-t', 'We\'ll escape ^_ ^ special chars...@{u}'],
        'desc\n\nBUG=\n\nChange-Id: I123456789',
        squash=False,
        squash_mode='override_nosquash',
        title='We%27ll_escape_%5E%5F_%5E_special_chars%2E%2E%2E%40%7Bu%7D')

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
        'desc\nTBR=reviewer@example.com\nBUG=\nR=another@example.com\n'
        'CC=more@example.com,people@example.com\n\n'
        'Change-Id: 123456789',
        ['reviewer@example.com', 'another@example.com'],
        expected_upstream_ref='origin/master',
        cc=['more@example.com', 'people@example.com'],
        tbr='reviewer@example.com')

  def test_gerrit_upload_squash_first_is_default(self):
    self._run_gerrit_upload_test(
        [],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        expected_upstream_ref='origin/master')

  def test_gerrit_upload_squash_first(self):
    self._run_gerrit_upload_test(
        ['--squash'],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        squash=True,
        expected_upstream_ref='origin/master')

  def test_gerrit_upload_squash_first_with_labels(self):
    self._run_gerrit_upload_test(
        ['--squash', '--cq-dry-run', '--enable-auto-submit'],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        squash=True,
        expected_upstream_ref='origin/master',
        labels={'Commit-Queue': 1, 'Auto-Submit': 1})

  def test_gerrit_upload_squash_first_against_rev(self):
    custom_cl_base = 'custom_cl_base_rev_or_branch'
    self._run_gerrit_upload_test(
        ['--squash', custom_cl_base],
        'desc\nBUG=\n\nChange-Id: 123456789',
        [],
        squash=True,
        expected_upstream_ref='origin/master',
        custom_cl_base=custom_cl_base)
    self.assertIn(
        'If you proceed with upload, more than 1 CL may be created by Gerrit',
        sys.stdout.getvalue())

  def test_gerrit_upload_squash_reupload(self):
    description = 'desc\nBUG=\n\nChange-Id: 123456789'
    self._run_gerrit_upload_test(
        ['--squash'],
        description,
        [],
        squash=True,
        expected_upstream_ref='origin/master',
        issue=123456)

  def test_gerrit_upload_squash_reupload_to_abandoned(self):
    self.mock(git_cl, 'DieWithError',
              lambda msg, change=None: self._mocked_call('DieWithError', msg))
    description = 'desc\nBUG=\n\nChange-Id: 123456789'
    with self.assertRaises(SystemExitMock):
      self._run_gerrit_upload_test(
          ['--squash'],
          description,
          [],
          squash=True,
          expected_upstream_ref='origin/master',
          issue=123456,
          fetched_status='ABANDONED')

  def test_gerrit_upload_squash_reupload_to_not_owned(self):
    self.mock(git_cl.gerrit_util, 'GetAccountDetails',
              lambda *_, **__: {'email': 'yet-another@example.com'})
    description = 'desc\nBUG=\n\nChange-Id: 123456789'
    self._run_gerrit_upload_test(
          ['--squash'],
          description,
          [],
          squash=True,
          expected_upstream_ref='origin/master',
          issue=123456,
          other_cl_owner='other@example.com')
    self.assertIn(
        'WARNING: Change 123456 is owned by other@example.com, but you '
        'authenticate to Gerrit as yet-another@example.com.\n'
        'Uploading may fail due to lack of permissions',
        git_cl.sys.stdout.getvalue())

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
        (('ask_for_data', 'This command will checkout all dependent branches '
                          'and run "git cl upload". Press Enter to continue, '
                          'or Ctrl+C to abort'), ''),
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
      ('foo', [], [],
       'foo'),
      ('foo\nR=xx', [], [],
       'foo\nR=xx'),
      ('foo\nTBR=xx', [], [],
       'foo\nTBR=xx'),
      ('foo', ['a@c'], [],
       'foo\n\nR=a@c'),
      ('foo\nR=xx', ['a@c'], [],
       'foo\n\nR=a@c, xx'),
      ('foo\nTBR=xx', ['a@c'], [],
       'foo\n\nR=a@c\nTBR=xx'),
      ('foo\nTBR=xx\nR=yy', ['a@c'], [],
       'foo\n\nR=a@c, yy\nTBR=xx'),
      ('foo\nBUG=', ['a@c'], [],
       'foo\nBUG=\nR=a@c'),
      ('foo\nR=xx\nTBR=yy\nR=bar', ['a@c'], [],
       'foo\n\nR=a@c, bar, xx\nTBR=yy'),
      ('foo', ['a@c', 'b@c'], [],
       'foo\n\nR=a@c, b@c'),
      ('foo\nBar\n\nR=\nBUG=', ['c@c'], [],
       'foo\nBar\n\nR=c@c\nBUG='),
      ('foo\nBar\n\nR=\nBUG=\nR=', ['c@c'], [],
       'foo\nBar\n\nR=c@c\nBUG='),
      # Same as the line before, but full of whitespaces.
      (
        'foo\nBar\n\n R = \n BUG = \n R = ', ['c@c'], [],
        'foo\nBar\n\nR=c@c\n BUG =',
      ),
      # Whitespaces aren't interpreted as new lines.
      ('foo BUG=allo R=joe ', ['c@c'], [],
       'foo BUG=allo R=joe\n\nR=c@c'),
      # Redundant TBRs get promoted to Rs
      ('foo\n\nR=a@c\nTBR=t@c', ['b@c', 'a@c'], ['a@c', 't@c'],
       'foo\n\nR=a@c, b@c\nTBR=t@c'),
    ]
    expected = [i[-1] for i in data]
    actual = []
    for orig, reviewers, tbrs, _expected in data:
      obj = git_cl.ChangeDescription(orig)
      obj.update_reviewers(reviewers, tbrs)
      actual.append(obj.description)
    self.assertEqual(expected, actual)

  def test_get_hash_tags(self):
    cases = [
      ('', []),
      ('a', []),
      ('[a]', ['a']),
      ('[aa]', ['aa']),
      ('[a ]', ['a']),
      ('[a- ]', ['a']),
      ('[a- b]', ['a-b']),
      ('[a--b]', ['a-b']),
      ('[a', []),
      ('[a]x', ['a']),
      ('[aa]x', ['aa']),
      ('[a b]', ['a-b']),
      ('[a  b]', ['a-b']),
      ('[a__b]', ['a-b']),
      ('[a] x', ['a']),
      ('[a][b]', ['a', 'b']),
      ('[a] [b]', ['a', 'b']),
      ('[a][b]x', ['a', 'b']),
      ('[a][b] x', ['a', 'b']),
      ('[a]\n[b]', ['a']),
      ('[a\nb]', []),
      ('[a][', ['a']),
      ('Revert "[a] feature"', ['a']),
      ('Reland "[a] feature"', ['a']),
      ('Revert: [a] feature', ['a']),
      ('Reland: [a] feature', ['a']),
      ('Revert "Reland: [a] feature"', ['a']),
      ('Foo: feature', ['foo']),
      ('Foo Bar: feature', ['foo-bar']),
      ('Revert "Foo bar: feature"', ['foo-bar']),
      ('Reland "Foo bar: feature"', ['foo-bar']),
    ]
    for desc, expected in cases:
      change_desc = git_cl.ChangeDescription(desc)
      actual = change_desc.get_hash_tags()
      self.assertEqual(
          actual,
          expected,
          'GetHashTags(%r) == %r, expected %r' % (desc, actual, expected))

  def test_get_target_ref(self):
    # Check remote or remote branch not present.
    self.assertEqual(None, git_cl.GetTargetRef('origin', None, 'master'))
    self.assertEqual(None, git_cl.GetTargetRef(None,
                                               'refs/remotes/origin/master',
                                               'master'))

    # Check default target refs for branches.
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/master',
                                         None))
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/lkgr',
                                         None))
    self.assertEqual('refs/heads/master',
                     git_cl.GetTargetRef('origin', 'refs/remotes/origin/lkcr',
                                         None))
    self.assertEqual('refs/branch-heads/123',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/branch-heads/123',
                                         None))
    self.assertEqual('refs/diff/test',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/origin/refs/diff/test',
                                         None))
    self.assertEqual('refs/heads/chrome/m42',
                     git_cl.GetTargetRef('origin',
                                         'refs/remotes/origin/chrome/m42',
                                         None))

    # Check target refs for user-specified target branch.
    for branch in ('branch-heads/123', 'remotes/branch-heads/123',
                   'refs/remotes/branch-heads/123'):
      self.assertEqual('refs/branch-heads/123',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/origin/master',
                                           branch))
    for branch in ('origin/master', 'remotes/origin/master',
                   'refs/remotes/origin/master'):
      self.assertEqual('refs/heads/master',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/branch-heads/123',
                                           branch))
    for branch in ('master', 'heads/master', 'refs/heads/master'):
      self.assertEqual('refs/heads/master',
                       git_cl.GetTargetRef('origin',
                                           'refs/remotes/branch-heads/123',
                                           branch))

  def test_patch_when_dirty(self):
    # Patch when local tree is dirty
    self.mock(git_common, 'is_dirty_git_tree', lambda x: True)
    self.assertNotEqual(git_cl.main(['patch', '123456']), 0)

  @staticmethod
  def _get_gerrit_codereview_server_calls(branch, value=None,
                                          git_short_host='host',
                                          detect_branch=True,
                                          detect_server=True):
    """Returns calls executed by _GerritChangelistImpl.GetCodereviewServer.

    If value is given, branch.<BRANCH>.gerritcodereview is already set.
    """
    calls = []
    if detect_branch:
      calls.append(((['git', 'symbolic-ref', 'HEAD'],), branch))
    if detect_server:
      calls.append(((['git', 'config', 'branch.' + branch + '.gerritserver'],),
                    CERR1 if value is None else value))
    if value is None:
      calls += [
          ((['git', 'config', 'branch.' + branch + '.merge'],),
           'refs/heads' + branch),
          ((['git', 'config', 'branch.' + branch + '.remote'],),
           'origin'),
          ((['git', 'config', 'remote.origin.url'],),
           'https://%s.googlesource.com/my/repo' % git_short_host),
      ]
    return calls

  def _patch_common(self, force_codereview=False,
                    new_branch=False, git_short_host='host',
                    detect_gerrit_server=False,
                    actual_codereview=None,
                    codereview_in_url=False):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl, 'IsGitVersionAtLeast', lambda *args: True)

    if new_branch:
      self.calls = [((['git', 'new-branch', 'master'],), ''),]

    if codereview_in_url and actual_codereview == 'rietveld':
      self.calls += [
        ((['git', 'rev-parse', '--show-cdup'],), ''),
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ]

    if not force_codereview and not codereview_in_url:
      # These calls detect codereview to use.
      self.calls += [
        ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
        ((['git', 'config', 'branch.master.gerritissue'],), CERR1),
        ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
        ((['git', 'config', 'gerrit.host'],), 'true'),
      ]
    if detect_gerrit_server:
      self.calls += self._get_gerrit_codereview_server_calls(
          'master', git_short_host=git_short_host,
          detect_branch=not new_branch and force_codereview)
      actual_codereview = 'gerrit'

    if actual_codereview == 'gerrit':
      self.calls += [
        (('GetChangeDetail', git_short_host + '-review.googlesource.com',
          'my%2Frepo~123456', ['ALL_REVISIONS', 'CURRENT_COMMIT']),
         {
           'current_revision': '7777777777',
           'revisions': {
             '1111111111': {
               '_number': 1,
               'fetch': {'http': {
                 'url': 'https://%s.googlesource.com/my/repo' % git_short_host,
                 'ref': 'refs/changes/56/123456/1',
               }},
             },
             '7777777777': {
               '_number': 7,
               'fetch': {'http': {
                 'url': 'https://%s.googlesource.com/my/repo' % git_short_host,
                 'ref': 'refs/changes/56/123456/7',
               }},
             },
           },
         }),
      ]

  def test_patch_gerrit_default(self):
    self._patch_common(git_short_host='chromium', detect_gerrit_server=True)
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],),
        ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://chromium-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '7'],), ''),
      ((['git', 'rev-parse', 'FETCH_HEAD'],), 'deadbeef'),
      ((['git', 'config', 'branch.master.last-upload-hash', 'deadbeef'],), ''),
      ((['git', 'config', 'branch.master.gerritsquashhash', 'deadbeef'],), ''),
    ]
    self.assertEqual(git_cl.main(['patch', '123456']), 0)

  def test_patch_gerrit_new_branch(self):
    self._patch_common(
        git_short_host='chromium', detect_gerrit_server=True, new_branch=True)
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],),
        ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://chromium-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '7'],), ''),
      ((['git', 'rev-parse', 'FETCH_HEAD'],), 'deadbeef'),
      ((['git', 'config', 'branch.master.last-upload-hash', 'deadbeef'],), ''),
      ((['git', 'config', 'branch.master.gerritsquashhash', 'deadbeef'],), ''),
    ]
    self.assertEqual(git_cl.main(['patch', '-b', 'master', '123456']), 0)

  def test_patch_gerrit_force(self):
    self._patch_common(
        force_codereview=True, git_short_host='host', detect_gerrit_server=True)
    self.calls += [
      ((['git', 'fetch', 'https://host.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'reset', '--hard', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],),
       ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://host-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '7'],), ''),
      ((['git', 'rev-parse', 'FETCH_HEAD'],), 'deadbeef'),
      ((['git', 'config', 'branch.master.last-upload-hash', 'deadbeef'],), ''),
      ((['git', 'config', 'branch.master.gerritsquashhash', 'deadbeef'],), ''),
    ]
    self.assertEqual(git_cl.main(['patch', '--gerrit', '123456', '--force']), 0)

  def test_patch_gerrit_guess_by_url(self):
    self.calls += self._get_gerrit_codereview_server_calls(
        'master', git_short_host='else', detect_server=False)
    self._patch_common(
        actual_codereview='gerrit', git_short_host='else',
        codereview_in_url=True, detect_gerrit_server=False)
    self.calls += [
      ((['git', 'fetch', 'https://else.googlesource.com/my/repo',
         'refs/changes/56/123456/1'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],),
       ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://else-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '1'],), ''),
      ((['git', 'rev-parse', 'FETCH_HEAD'],), 'deadbeef'),
      ((['git', 'config', 'branch.master.last-upload-hash', 'deadbeef'],), ''),
      ((['git', 'config', 'branch.master.gerritsquashhash', 'deadbeef'],), ''),
    ]
    self.assertEqual(git_cl.main(
      ['patch', 'https://else-review.googlesource.com/#/c/123456/1']), 0)

  def test_patch_gerrit_guess_by_url_with_repo(self):
    self.calls += self._get_gerrit_codereview_server_calls(
        'master', git_short_host='else', detect_server=False)
    self._patch_common(
        actual_codereview='gerrit', git_short_host='else',
        codereview_in_url=True, detect_gerrit_server=False)
    self.calls += [
      ((['git', 'fetch', 'https://else.googlesource.com/my/repo',
         'refs/changes/56/123456/1'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), ''),
      ((['git', 'config', 'branch.master.gerritissue', '123456'],),
       ''),
      ((['git', 'config', 'branch.master.gerritserver',
        'https://else-review.googlesource.com'],), ''),
      ((['git', 'config', 'branch.master.gerritpatchset', '1'],), ''),
      ((['git', 'rev-parse', 'FETCH_HEAD'],), 'deadbeef'),
      ((['git', 'config', 'branch.master.last-upload-hash', 'deadbeef'],), ''),
      ((['git', 'config', 'branch.master.gerritsquashhash', 'deadbeef'],), ''),
    ]
    self.assertEqual(git_cl.main(
      ['patch', 'https://else-review.googlesource.com/c/my/repo/+/123456/1']),
      0)

  def test_patch_gerrit_conflict(self):
    self._patch_common(detect_gerrit_server=True, git_short_host='chromium')
    self.calls += [
      ((['git', 'fetch', 'https://chromium.googlesource.com/my/repo',
         'refs/changes/56/123456/7'],), ''),
      ((['git', 'cherry-pick', 'FETCH_HEAD'],), CERR1),
      ((['DieWithError', 'Command "git cherry-pick FETCH_HEAD" failed.\n'],),
       SystemExitMock()),
    ]
    with self.assertRaises(SystemExitMock):
      git_cl.main(['patch', '123456'])

  def test_patch_gerrit_not_exists(self):
    def notExists(_issue, *_, **kwargs):
      raise git_cl.gerrit_util.GerritError(404, '')
    self.mock(git_cl.gerrit_util, 'GetChangeDetail', notExists)

    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.gerritissue'],), CERR1),
      ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
      ((['git', 'config', 'gerrit.host'],), 'true'),
      ((['git', 'config', 'branch.master.gerritserver'],), CERR1),
      ((['git', 'config', 'branch.master.merge'],), 'refs/heads/master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/my/repo'),
      ((['DieWithError',
         'change 123456 at https://chromium-review.googlesource.com does not '
         'exist or you have no access to it'],), SystemExitMock()),
    ]
    with self.assertRaises(SystemExitMock):
      self.assertEqual(1, git_cl.main(['patch', '123456']))

  def _checkout_calls(self):
    return [
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
           'branch\\..*\\.gerritissue'], ), CERR1),
    ]
    self.assertEqual(1, git_cl.main(['checkout', '99999']))

  def _test_gerrit_ensure_authenticated_common(self, auth,
                                               skip_auth_check=False):
    self.mock(git_cl.gerrit_util, 'CookiesAuthenticator',
              CookiesAuthenticatorMockFactory(hosts_with_creds=auth))
    self.mock(git_cl, 'DieWithError',
              lambda msg, change=None: self._mocked_call(['DieWithError', msg]))
    self.calls = self._gerrit_ensure_auth_calls(skip_auth_check=skip_auth_check)
    cl = git_cl.Changelist(codereview='gerrit')
    cl.branch = 'master'
    cl.branchref = 'refs/heads/master'
    cl.lookedup_issue = True
    return cl

  def test_gerrit_ensure_authenticated_missing(self):
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com': ('git-is.ok', '', 'but gerrit is missing'),
    })
    self.calls.append(
        ((['DieWithError',
           'Credentials for the following hosts are required:\n'
           '  chromium-review.googlesource.com\n'
           'These are read from ~/.gitcookies (or legacy ~/.netrc)\n'
           'You can (re)generate your credentials by visiting '
           'https://chromium-review.googlesource.com/new-password'],), ''),)
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_conflict(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com':
          ('git-one.example.com', None, 'secret1'),
      'chromium-review.googlesource.com':
          ('git-other.example.com', None, 'secret2'),
    })
    self.calls.append(
        (('ask_for_data', 'If you know what you are doing '
                          'press Enter to continue, or Ctrl+C to abort'), ''))
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_ok(self):
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com':
          ('git-same.example.com', None, 'secret'),
      'chromium-review.googlesource.com':
          ('git-same.example.com', None, 'secret'),
    })
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_skipped(self):
    cl = self._test_gerrit_ensure_authenticated_common(
        auth={}, skip_auth_check=True)
    self.assertIsNone(cl.EnsureAuthenticated(force=False))

  def test_gerrit_ensure_authenticated_bearer_token(self):
    cl = self._test_gerrit_ensure_authenticated_common(auth={
      'chromium.googlesource.com':
          ('', None, 'secret'),
      'chromium-review.googlesource.com':
          ('', None, 'secret'),
    })
    self.assertIsNone(cl.EnsureAuthenticated(force=False))
    header = gerrit_util.CookiesAuthenticator().get_auth_header(
        'chromium.googlesource.com')
    self.assertTrue('Bearer' in header)

  def _cmd_set_commit_gerrit_common(self, vote, notify=None):
    self.mock(git_cl.gerrit_util, 'SetReview',
              lambda h, i, labels, notify=None:
                  self._mocked_call(['SetReview', h, i, labels, notify]))

    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', 'branch.feature.gerritserver'],),
         'https://chromium-review.googlesource.com'),
        ((['git', 'config', 'branch.feature.merge'],), 'refs/heads/master'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/infra/infra.git'),
        ((['SetReview', 'chromium-review.googlesource.com',
           'infra%2Finfra~123',
           {'Commit-Queue': vote}, notify],), ''),
    ]

  def test_cmd_set_commit_gerrit_clear(self):
    self._cmd_set_commit_gerrit_common(0)
    self.assertEqual(0, git_cl.main(['set-commit', '-c']))

  def test_cmd_set_commit_gerrit_dry(self):
    self._cmd_set_commit_gerrit_common(1, notify=False)
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

  def test_StatusFieldOverrideIssueMissingArgs(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stderr', out)

    try:
      self.assertEqual(git_cl.main(['status', '--issue', '1']), 0)
    except SystemExit as ex:
      self.assertEqual(ex.code, 2)
      self.assertRegexpMatches(out.getvalue(), r'--issue must be specified')

    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stderr', out)

    try:
      self.assertEqual(git_cl.main(['status', '--issue', '1', '--gerrit']), 0)
    except SystemExit as ex:
      self.assertEqual(ex.code, 2)
      self.assertRegexpMatches(out.getvalue(), r'--field must be specified')

  def test_StatusFieldOverrideIssue(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)

    def assertIssue(cl_self, *_args):
      self.assertEquals(cl_self.issue, 1)
      return 'foobar'

    self.mock(git_cl.Changelist, 'GetDescription', assertIssue)
    self.assertEqual(
      git_cl.main(['status', '--issue', '1', '--gerrit', '--field', 'desc']),
      0)
    self.assertEqual(out.getvalue(), 'foobar\n')

  def test_SetCloseOverrideIssue(self):
    def assertIssue(cl_self, *_args):
      self.assertEquals(cl_self.issue, 1)
      return 'foobar'

    self.mock(git_cl.Changelist, 'GetDescription', assertIssue)
    self.mock(git_cl.Changelist, 'CloseIssue', lambda *_: None)
    self.assertEqual(
      git_cl.main(['set-close', '--issue', '1', '--gerrit']), 0)

  def test_description(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.merge'],), 'feature'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/my/repo'),
        (('GetChangeDetail', 'chromium-review.googlesource.com',
          'my%2Frepo~123123', ['CURRENT_REVISION', 'CURRENT_COMMIT']),
         {
           'current_revision': 'sha1',
           'revisions': {'sha1': {
             'commit': {'message': 'foobar'},
           }},
         }),
    ]
    self.assertEqual(0, git_cl.main([
        'description',
        'https://chromium-review.googlesource.com/c/my/repo/+/123123',
        '-d']))
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
          'Some.\n\nChange-Id: xxx\nBug: ',
          desc)
      # Simulate user changing something.
      return 'Some.\n\nChange-Id: xxx\nBug: 123'

    def UpdateDescriptionRemote(_, desc, force=False):
      self.assertEquals(desc, 'Some.\n\nChange-Id: xxx\nBug: 123')

    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda *args: current_desc)
    self.mock(git_cl._GerritChangelistImpl, 'UpdateDescriptionRemote',
              UpdateDescriptionRemote)
    self.mock(git_cl.gclient_utils, 'RunEditor', RunEditor)

    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
        ((['git', 'config', 'rietveld.bug-prefix'],), CERR1),
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
         ((['git', 'config', 'branch.master.gerritissue'],), '456'),
         ((['git', 'config', 'branch.foo.gerritissue'],), CERR1),
         ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
         ((['git', 'config', 'gerrit.host'],), 'true'),
         ((['git', 'config', 'branch.bar.gerritissue'],), '789'),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
         ((['git', 'tag', 'git-cl-archived-456-foo', 'foo'],), ''),
         ((['git', 'branch', '-D', 'foo'],), '')]

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelistWithBranchAndIssue('master', 1), 'open'),
               (MockChangelistWithBranchAndIssue('foo', 456), 'closed'),
               (MockChangelistWithBranchAndIssue('bar', 789), 'open')])

    self.assertEqual(0, git_cl.main(['archive', '-f']))

  def test_archive_current_branch_fails(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.calls = \
        [((['git', 'for-each-ref', '--format=%(refname)', 'refs/heads'],),
          'refs/heads/master'),
         ((['git', 'config', 'branch.master.gerritissue'],), '1'),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master')]

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelistWithBranchAndIssue('master', 1), 'closed')])

    self.assertEqual(1, git_cl.main(['archive', '-f']))

  def test_archive_dry_run(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())

    self.calls = \
        [((['git', 'for-each-ref', '--format=%(refname)', 'refs/heads'],),
          'refs/heads/master\nrefs/heads/foo\nrefs/heads/bar'),
         ((['git', 'config', 'branch.master.gerritissue'],), '456'),
         ((['git', 'config', 'branch.foo.gerritissue'],), CERR1),
         ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
         ((['git', 'config', 'gerrit.host'],), 'true'),
         ((['git', 'config', 'branch.bar.gerritissue'],), '789'),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master'),]

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelistWithBranchAndIssue('master', 1), 'open'),
               (MockChangelistWithBranchAndIssue('foo', 456), 'closed'),
               (MockChangelistWithBranchAndIssue('bar', 789), 'open')])

    self.assertEqual(0, git_cl.main(['archive', '-f', '--dry-run']))

  def test_archive_no_tags(self):
    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())

    self.calls = \
        [((['git', 'for-each-ref', '--format=%(refname)', 'refs/heads'],),
          'refs/heads/master\nrefs/heads/foo\nrefs/heads/bar'),
         ((['git', 'config', 'branch.master.gerritissue'],), '1'),
         ((['git', 'config', 'branch.foo.gerritissue'],), '456'),
         ((['git', 'config', 'branch.bar.gerritissue'],), CERR1),
         ((['git', 'config', 'rietveld.autoupdate'],), CERR1),
         ((['git', 'config', 'gerrit.host'],), 'true'),
         ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
         ((['git', 'branch', '-D', 'foo'],), '')]

    self.mock(git_cl, 'get_cl_statuses',
              lambda branches, fine_grained, max_processes:
              [(MockChangelistWithBranchAndIssue('master', 1), 'open'),
               (MockChangelistWithBranchAndIssue('foo', 456), 'closed'),
               (MockChangelistWithBranchAndIssue('bar', 789), 'open')])

    self.assertEqual(0, git_cl.main(['archive', '-f', '--notags']))

  def test_cmd_issue_erase_existing(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        # Let this command raise exception (retcode=1) - it should be ignored.
        ((['git', 'config', '--unset', 'branch.feature.last-upload-hash'],),
         CERR1),
        ((['git', 'config', '--unset', 'branch.feature.gerritissue'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritpatchset'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritserver'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritsquashhash'],),
         ''),
        ((['git', 'log', '-1', '--format=%B'],), 'This is a description'),
    ]
    self.assertEqual(0, git_cl.main(['issue', '0']))

  def test_cmd_issue_erase_existing_with_change_id(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.mock(git_cl.Changelist, 'GetDescription',
              lambda _: 'This is a description\n\nChange-Id: Ideadbeef')
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        # Let this command raise exception (retcode=1) - it should be ignored.
        ((['git', 'config', '--unset', 'branch.feature.last-upload-hash'],),
         CERR1),
        ((['git', 'config', '--unset', 'branch.feature.gerritissue'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritpatchset'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritserver'],), ''),
        ((['git', 'config', '--unset', 'branch.feature.gerritsquashhash'],),
         ''),
        ((['git', 'log', '-1', '--format=%B'],),
         'This is a description\n\nChange-Id: Ideadbeef'),
        ((['git', 'commit', '--amend', '-m', 'This is a description\n'],), ''),
    ]
    self.assertEqual(0, git_cl.main(['issue', '0']))

  def test_cmd_issue_json(self):
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123'),
        ((['git', 'config', 'branch.feature.gerritserver'],),
         'https://chromium-review.googlesource.com'),
        (('write_json', 'output.json',
          {'issue': 123,
           'issue_url': 'https://chromium-review.googlesource.com/123'}),
         ''),
    ]
    self.assertEqual(0, git_cl.main(['issue', '--json', 'output.json']))

  def test_git_cl_try_default_cq_dry_run_gerrit(self):
    self.mock(git_cl.Changelist, 'GetChange',
              lambda _, *a: (
                self._mocked_call(['GetChange']+list(a))))
    self.mock(git_cl.presubmit_support, 'DoGetTryMasters',
              lambda *_, **__: (
                self._mocked_call(['DoGetTryMasters'])))
    self.mock(git_cl._GerritChangelistImpl, 'SetCQState',
              lambda _, s: self._mocked_call(['SetCQState', s]))

    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123456'),
        ((['git', 'config', 'branch.feature.gerritserver'],),
         'https://chromium-review.googlesource.com'),
        ((['git', 'config', 'branch.feature.merge'],), 'feature'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/depot_tools'),
        (('GetChangeDetail', 'chromium-review.googlesource.com',
          'depot_tools~123456',
         ['DETAILED_ACCOUNTS', 'ALL_REVISIONS', 'CURRENT_COMMIT']), {
          'project': 'depot_tools',
          'status': 'OPEN',
          'owner': {'email': 'owner@e.mail'},
          'revisions': {
            'deadbeaf':  {
              '_number': 6,
            },
            'beeeeeef': {
              '_number': 7,
              'fetch': {'http': {
                'url': 'https://chromium.googlesource.com/depot_tools',
                'ref': 'refs/changes/56/123456/7'
              }},
            },
          },
        }),
        ((['git', 'config', 'branch.feature.merge'],), 'feature'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['get_or_create_merge_base', 'feature', 'feature'],),
         'fake_ancestor_sha'),
        ((['GetChange', 'fake_ancestor_sha', None], ),
         git_cl.presubmit_support.GitChange(
           '', '', '', '', '', '', '', '')),
        ((['git', 'rev-parse', '--show-cdup'],), '../'),
        ((['DoGetTryMasters'], ), None),
        ((['SetCQState', git_cl._CQState.DRY_RUN], ), None),
    ]
    out = StringIO.StringIO()
    self.mock(git_cl.sys, 'stdout', out)
    self.assertEqual(0, git_cl.main(['try']))
    self.assertEqual(
        out.getvalue(),
        'Scheduling CQ dry run on: '
        'https://chromium-review.googlesource.com/123456\n')

  def test_git_cl_try_buildbucket_with_properties_gerrit(self):
    self.mock(git_cl.Changelist, 'GetMostRecentPatchset', lambda _: 7)
    self.mock(git_cl.uuid, 'uuid4', lambda: 'uuid4')

    self.calls = [
        ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
        ((['git', 'config', 'branch.feature.gerritissue'],), '123456'),
        ((['git', 'config', 'branch.feature.gerritserver'],),
         'https://chromium-review.googlesource.com'),
        ((['git', 'config', 'branch.feature.merge'],), 'feature'),
        ((['git', 'config', 'branch.feature.remote'],), 'origin'),
        ((['git', 'config', 'remote.origin.url'],),
         'https://chromium.googlesource.com/depot_tools'),
        (('GetChangeDetail', 'chromium-review.googlesource.com',
          'depot_tools~123456',
         ['DETAILED_ACCOUNTS', 'ALL_REVISIONS', 'CURRENT_COMMIT']), {
          'project': 'depot_tools',
          'status': 'OPEN',
          'owner': {'email': 'owner@e.mail'},
          'revisions': {
            'deadbeaf':  {
              '_number': 6,
            },
            'beeeeeef': {
              '_number': 7,
              'fetch': {'http': {
                'url': 'https://chromium.googlesource.com/depot_tools',
                'ref': 'refs/changes/56/123456/7'
              }},
            },
          },
        }),
    ]

    def _buildbucket_retry(*_, **kw):
      # self.maxDiff = 10000
      body = json.loads(kw['body'])
      self.assertEqual(len(body['builds']), 1)
      build = body['builds'][0]
      params = json.loads(build.pop('parameters_json'))
      self.assertEqual(params, {
        u'builder_name': u'win',
        u'changes': [{u'author': {u'email': u'owner@e.mail'},
                      u'revision': None}],
        u'properties': {
          u'category': u'git_cl_try',
           u'key': u'val',
           u'json': [{u'a': 1}, None],

           u'patch_gerrit_url':
             u'https://chromium-review.googlesource.com',
           u'patch_issue': 123456,
           u'patch_project': u'depot_tools',
           u'patch_ref': u'refs/changes/56/123456/7',
           u'patch_repository_url':
             u'https://chromium.googlesource.com/depot_tools',
           u'patch_set': 7,
           u'patch_storage': u'gerrit',
        }
      })
      self.assertEqual(build, {
        u'bucket': u'luci.chromium.try',
        u'client_operation_id': u'uuid4',
        u'tags': [
          u'builder:win',
          u'buildset:patch/gerrit/chromium-review.googlesource.com/123456/7',
          u'user_agent:git_cl_try',
        ],
      })

    self.mock(git_cl, '_buildbucket_retry', _buildbucket_retry)

    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.assertEqual(0, git_cl.main([
        'try', '-B', 'luci.chromium.try', '-b', 'win',
        '-p', 'key=val', '-p', 'json=[{"a":1}, null]']))
    self.assertRegexpMatches(
        git_cl.sys.stdout.getvalue(),
        'Tried jobs on:\nBucket: luci.chromium.try')

  def test_git_cl_try_bots_on_multiple_masters(self):
    self.mock(git_cl.Changelist, 'GetMostRecentPatchset', lambda _: 7)
    self.mock(git_cl.Changelist, 'GetChange',
              lambda _, *a: (
                self._mocked_call(['GetChange']+list(a))))
    self.mock(git_cl.presubmit_support, 'DoGetTryMasters',
              lambda *_, **__: (
                self._mocked_call(['DoGetTryMasters'])))
    self.mock(git_cl._GerritChangelistImpl, 'SetCQState',
              lambda _, s: self._mocked_call(['SetCQState', s]))

    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
      ((['git', 'config', 'branch.feature.gerritissue'],), '123456'),
      ((['git', 'config', 'branch.feature.gerritserver'],),
       'https://chromium-review.googlesource.com'),
      ((['git', 'config', 'branch.feature.merge'],), 'feature'),
      ((['git', 'config', 'branch.feature.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/depot_tools'),
      (('GetChangeDetail', 'chromium-review.googlesource.com',
        'depot_tools~123456',
       ['DETAILED_ACCOUNTS', 'ALL_REVISIONS', 'CURRENT_COMMIT']), {
        'project': 'depot_tools',
        'status': 'OPEN',
        'owner': {'email': 'owner@e.mail'},
        'revisions': {
          'deadbeaf':  {
            '_number': 6,
          },
          'beeeeeef': {
            '_number': 7,
            'fetch': {'http': {
              'url': 'https://chromium.googlesource.com/depot_tools',
              'ref': 'refs/changes/56/123456/7'
            }},
          },
        },
      }),
    ]

    def _buildbucket_retry(*_, **kw):
      body = json.loads(kw['body'])
      self.assertEqual(len(body['builds']), 2)

      self.assertEqual(body['builds'][0]['bucket'], 'bucket1')
      params = json.loads(body['builds'][0]['parameters_json'])
      self.assertEqual(params['builder_name'], 'builder1')

      self.assertEqual(body['builds'][1]['bucket'], 'bucket2')
      params = json.loads(body['builds'][1]['parameters_json'])
      self.assertEqual(params['builder_name'], 'builder2')

    self.mock(git_cl, '_buildbucket_retry', _buildbucket_retry)

    self.mock(git_cl.urllib2, 'urlopen', lambda _: StringIO.StringIO(
      json.dumps({
        'builder1': {'bucket': 'bucket1'},
        'builder2': {'bucket': 'bucket2'},
      })))

    self.mock(git_cl.sys, 'stdout', StringIO.StringIO())
    self.assertEqual(
        0, git_cl.main(['try', '-b', 'builder1', '-b', 'builder2']))
    self.assertEqual(
        git_cl.sys.stdout.getvalue(),
        'Tried jobs on:\n'
        'Bucket: bucket1\n'
        '  builder1: []\n'
        'Bucket: bucket2\n'
        '  builder2: []\n'
        'To see results here, run:        git cl try-results\n'
        'To see results in browser, run:  git cl web\n')

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
        (('ask_for_data', 'Do you want to remove it now? [Yes/No]: '), 'Yes'),
        ((['rm_file_or_tree', '/abs/git_repo_root/.git/hooks/commit-msg'],),
         ''),
    ]
    cl._codereview_impl._GerritCommitMsgHookCheck(offer_removal=True)

  def test_GerritCmdLand(self):
    self.calls += [
      ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
      ((['git', 'config', 'branch.feature.gerritsquashhash'],),
       'deadbeaf'),
      ((['git', 'diff', 'deadbeaf'],), ''),  # No diff.
      ((['git', 'config', 'branch.feature.gerritserver'],),
       'chromium-review.googlesource.com'),
    ]
    cl = git_cl.Changelist(issue=123, codereview='gerrit')
    cl._codereview_impl._GetChangeDetail = lambda _: {
      'labels': {},
      'current_revision': 'deadbeaf',
    }
    cl._codereview_impl._GetChangeCommit = lambda: {
      'commit': 'deadbeef',
      'web_links': [{'name': 'gitiles',
                     'url': 'https://git.googlesource.com/test/+/deadbeef'}],
    }
    cl._codereview_impl.SubmitIssue = lambda wait_for_merge: None
    out = StringIO.StringIO()
    self.mock(sys, 'stdout', out)
    self.assertEqual(0, cl.CMDLand(force=True,
                                   bypass_hooks=True,
                                   verbose=True,
                                   parallel=False))
    self.assertRegexpMatches(out.getvalue(), 'Issue.*123 has been submitted')
    self.assertRegexpMatches(out.getvalue(), 'Landed as: .*deadbeef')

  BUILDBUCKET_BUILDS_MAP = {
    '9000': {
      'id': '9000',
      'bucket': 'master.x.y',
      'created_by': 'user:someone@chromium.org',
      'created_ts': '147200002222000',
      'experimental': False,
      'parameters_json': json.dumps({
        'builder_name': 'my-bot',
        'properties': {'category': 'cq'},
      }),
      'status': 'STARTED',
      'tags': [
        'build_address:x.y/my-bot/2',
        'builder:my-bot',
        'experimental:false',
        'user_agent:cq',
      ],
      'url': 'http://build.cr.org/p/x.y/builders/my-bot/builds/2',
    },
    '8000': {
      'id': '8000',
      'bucket': 'master.x.y',
      'created_by': 'user:someone@chromium.org',
      'created_ts': '147200001111000',
      'experimental': False,
      'failure_reason': 'BUILD_FAILURE',
      'parameters_json': json.dumps({
        'builder_name': 'my-bot',
        'properties': {'category': 'cq'},
      }),
      'result_details_json': json.dumps({
        'properties': {'buildnumber': 1},
      }),
      'result': 'FAILURE',
      'status': 'COMPLETED',
      'tags': [
        'build_address:x.y/my-bot/1',
        'builder:my-bot',
        'experimental:false',
        'user_agent:cq',
      ],
      'url': 'http://build.cr.org/p/x.y/builders/my-bot/builds/1',
    },
  }

  def test_write_try_results_json(self):
    expected_output = [
      {
        'bucket': 'master.x.y',
        'buildbucket_id': '8000',
        'builder_name': 'my-bot',
        'created_ts': '147200001111000',
        'experimental': False,
        'failure_reason': 'BUILD_FAILURE',
        'result': 'FAILURE',
        'status': 'COMPLETED',
        'tags': [
          'build_address:x.y/my-bot/1',
          'builder:my-bot',
          'experimental:false',
          'user_agent:cq',
        ],
        'url': 'http://build.cr.org/p/x.y/builders/my-bot/builds/1',
      },
      {
        'bucket': 'master.x.y',
        'buildbucket_id': '9000',
        'builder_name': 'my-bot',
        'created_ts': '147200002222000',
        'experimental': False,
        'failure_reason': None,
        'result': None,
        'status': 'STARTED',
        'tags': [
          'build_address:x.y/my-bot/2',
          'builder:my-bot',
          'experimental:false',
          'user_agent:cq',
        ],
        'url': 'http://build.cr.org/p/x.y/builders/my-bot/builds/2',
      },
    ]
    self.calls = [(('write_json', 'output.json', expected_output), '')]
    git_cl.write_try_results_json('output.json', self.BUILDBUCKET_BUILDS_MAP)

  def _setup_fetch_try_jobs(self, most_recent_patchset=20001):
    out = StringIO.StringIO()
    self.mock(sys, 'stdout', out)
    self.mock(git_cl.Changelist, 'GetMostRecentPatchset',
              lambda *args: most_recent_patchset)
    self.mock(git_cl.auth, 'get_authenticator_for_host', lambda host, _cfg:
              self._mocked_call(['get_authenticator_for_host', host]))
    self.mock(git_cl, '_buildbucket_retry', lambda *_, **__:
              self._mocked_call(['_buildbucket_retry']))

  def _setup_fetch_try_jobs_gerrit(self, *request_results):
    self._setup_fetch_try_jobs(most_recent_patchset=13)
    self.calls += [
      ((['git', 'symbolic-ref', 'HEAD'],), 'feature'),
      ((['git', 'config', 'branch.feature.gerritissue'],), '1'),
      # TODO(tandrii): Uncomment the below if we decide to support checking
      # patchsets for Gerrit.
      # Simulate that Gerrit has more patchsets than local.
      # ((['git', 'config', 'branch.feature.gerritpatchset'],), '12'),
      ((['git', 'config', 'branch.feature.gerritserver'],),
       'https://x-review.googlesource.com'),
      ((['get_authenticator_for_host', 'x-review.googlesource.com'],),
       AuthenticatorMock()),
    ] + [((['_buildbucket_retry'],), r) for r in request_results]

  def test_fetch_try_jobs_none_gerrit(self):
    self._setup_fetch_try_jobs_gerrit({})
    self.assertEqual(0, git_cl.main(['try-results']))
    # TODO(tandrii): Uncomment the below if we decide to support checking
    # patchsets for Gerrit.
    # self.assertRegexpMatches(
    #     sys.stdout.getvalue(),
    #     r'Warning: Codereview server has newer patchsets \(13\)')
    self.assertRegexpMatches(sys.stdout.getvalue(), 'No try jobs')

  def test_fetch_try_jobs_some_gerrit(self):
    self._setup_fetch_try_jobs_gerrit({
      'builds': self.BUILDBUCKET_BUILDS_MAP.values(),
    })
    # TODO(tandrii): Uncomment the below if we decide to support checking
    # patchsets for Gerrit.
    # self.calls.remove(
    #     ((['git', 'config', 'branch.feature.gerritpatchset'],), '12'))
    self.assertEqual(0, git_cl.main(['try-results', '--patchset', '5']))

    # ... and doesn't result in warning.
    self.assertNotRegexpMatches(sys.stdout.getvalue(), 'Warning')
    self.assertRegexpMatches(sys.stdout.getvalue(), '^Failures:')
    self.assertRegexpMatches(sys.stdout.getvalue(), 'Started:')
    self.assertRegexpMatches(sys.stdout.getvalue(), '2 try jobs')

  def _mock_gerrit_changes_for_detail_cache(self):
    self.mock(git_cl._GerritChangelistImpl, '_GetGerritHost', lambda _: 'host')

  def test_gerrit_change_detail_cache_simple(self):
    self._mock_gerrit_changes_for_detail_cache()
    self.calls = [
        (('GetChangeDetail', 'host', 'my%2Frepo~1', []), 'a'),
        (('GetChangeDetail', 'host', 'ab%2Frepo~2', []), 'b'),
        (('GetChangeDetail', 'host', 'ab%2Frepo~2', []), 'b2'),
    ]
    cl1 = git_cl.Changelist(issue=1, codereview='gerrit')
    cl1._cached_remote_url = (
        True, 'https://chromium.googlesource.com/a/my/repo.git/')
    cl2 = git_cl.Changelist(issue=2, codereview='gerrit')
    cl2._cached_remote_url = (
        True, 'https://chromium.googlesource.com/ab/repo')
    self.assertEqual(cl1._GetChangeDetail(), 'a')  # Miss.
    self.assertEqual(cl1._GetChangeDetail(), 'a')
    self.assertEqual(cl2._GetChangeDetail(), 'b')  # Miss.
    self.assertEqual(cl2._GetChangeDetail(no_cache=True), 'b2')  # Miss.
    self.assertEqual(cl1._GetChangeDetail(), 'a')
    self.assertEqual(cl2._GetChangeDetail(), 'b2')

  def test_gerrit_change_detail_cache_options(self):
    self._mock_gerrit_changes_for_detail_cache()
    self.calls = [
        (('GetChangeDetail', 'host', 'repo~1', ['C', 'A', 'B']), 'cab'),
        (('GetChangeDetail', 'host', 'repo~1', ['A', 'D']), 'ad'),
        (('GetChangeDetail', 'host', 'repo~1', ['A']), 'a'),  # no_cache=True
        # no longer in cache.
        (('GetChangeDetail', 'host', 'repo~1', ['B']), 'b'),
    ]
    cl = git_cl.Changelist(issue=1, codereview='gerrit')
    cl._cached_remote_url = (True, 'https://chromium.googlesource.com/repo/')
    self.assertEqual(cl._GetChangeDetail(options=['C', 'A', 'B']), 'cab')
    self.assertEqual(cl._GetChangeDetail(options=['A', 'B', 'C']), 'cab')
    self.assertEqual(cl._GetChangeDetail(options=['B', 'A']), 'cab')
    self.assertEqual(cl._GetChangeDetail(options=['C']), 'cab')
    self.assertEqual(cl._GetChangeDetail(options=['A']), 'cab')
    self.assertEqual(cl._GetChangeDetail(), 'cab')

    self.assertEqual(cl._GetChangeDetail(options=['A', 'D']), 'ad')
    self.assertEqual(cl._GetChangeDetail(options=['A']), 'cab')
    self.assertEqual(cl._GetChangeDetail(options=['D']), 'ad')
    self.assertEqual(cl._GetChangeDetail(), 'cab')

    # Finally, no_cache should invalidate all caches for given change.
    self.assertEqual(cl._GetChangeDetail(options=['A'], no_cache=True), 'a')
    self.assertEqual(cl._GetChangeDetail(options=['B']), 'b')

  def test_gerrit_description_caching(self):
    def gen_detail(rev, desc):
      return {
        'current_revision': rev,
        'revisions': {rev: {'commit': {'message': desc}}}
      }
    self.calls = [
        (('GetChangeDetail', 'host', 'my%2Frepo~1',
          ['CURRENT_REVISION', 'CURRENT_COMMIT']),
         gen_detail('rev1', 'desc1')),
        (('GetChangeDetail', 'host', 'my%2Frepo~1',
          ['CURRENT_REVISION', 'CURRENT_COMMIT']),
         gen_detail('rev2', 'desc2')),
    ]

    self._mock_gerrit_changes_for_detail_cache()
    cl = git_cl.Changelist(issue=1, codereview='gerrit')
    cl._cached_remote_url = (
        True, 'https://chromium.googlesource.com/a/my/repo.git/')
    self.assertEqual(cl.GetDescription(), 'desc1')
    self.assertEqual(cl.GetDescription(), 'desc1')  # cache hit.
    self.assertEqual(cl.GetDescription(force=True), 'desc2')

  def test_print_current_creds(self):
    class CookiesAuthenticatorMock(object):
      def __init__(self):
        self.gitcookies = {
            'host.googlesource.com': ('user', 'pass'),
            'host-review.googlesource.com': ('user', 'pass'),
        }
        self.netrc = self
        self.netrc.hosts = {
            'github.com': ('user2', None, 'pass2'),
            'host2.googlesource.com': ('user3', None, 'pass'),
        }
    self.mock(git_cl.gerrit_util, 'CookiesAuthenticator',
              CookiesAuthenticatorMock)
    self.mock(sys, 'stdout', StringIO.StringIO())
    git_cl._GitCookiesChecker().print_current_creds(include_netrc=True)
    self.assertEqual(list(sys.stdout.getvalue().splitlines()), [
        '                        Host\t User\t Which file',
        '============================\t=====\t===========',
        'host-review.googlesource.com\t user\t.gitcookies',
        '       host.googlesource.com\t user\t.gitcookies',
        '      host2.googlesource.com\tuser3\t     .netrc',
    ])
    sys.stdout.buf = ''
    git_cl._GitCookiesChecker().print_current_creds(include_netrc=False)
    self.assertEqual(list(sys.stdout.getvalue().splitlines()), [
        '                        Host\tUser\t Which file',
        '============================\t====\t===========',
        'host-review.googlesource.com\tuser\t.gitcookies',
        '       host.googlesource.com\tuser\t.gitcookies',
    ])

  def _common_creds_check_mocks(self):
    def exists_mock(path):
      dirname = os.path.dirname(path)
      if dirname == os.path.expanduser('~'):
        dirname = '~'
      base = os.path.basename(path)
      if base in ('.netrc', '.gitcookies'):
        return self._mocked_call('os.path.exists', '%s/%s' % (dirname, base))
      # git cl also checks for existence other files not relevant to this test.
      return None
    self.mock(os.path, 'exists', exists_mock)
    self.mock(sys, 'stdout', StringIO.StringIO())

  def test_creds_check_gitcookies_not_configured(self):
    self._common_creds_check_mocks()
    self.mock(git_cl._GitCookiesChecker, 'get_hosts_with_creds',
              lambda _, include_netrc=False: [])
    self.calls = [
      ((['git', 'config', '--path', 'http.cookiefile'],), CERR1),
      ((['git', 'config', '--global', 'http.cookiefile'],), CERR1),
      (('os.path.exists', '~/.netrc'), True),
      (('ask_for_data', 'Press Enter to setup .gitcookies, '
        'or Ctrl+C to abort'), ''),
      ((['git', 'config', '--global', 'http.cookiefile',
         os.path.expanduser('~/.gitcookies')], ), ''),
    ]
    self.assertEqual(0, git_cl.main(['creds-check']))
    self.assertRegexpMatches(
        sys.stdout.getvalue(),
        '^You seem to be using outdated .netrc for git credentials:')
    self.assertRegexpMatches(
        sys.stdout.getvalue(),
        '\nConfigured git to use .gitcookies from')

  def test_creds_check_gitcookies_configured_custom_broken(self):
    self._common_creds_check_mocks()
    self.mock(git_cl._GitCookiesChecker, 'get_hosts_with_creds',
              lambda _, include_netrc=False: [])
    self.calls = [
      ((['git', 'config', '--path', 'http.cookiefile'],), CERR1),
      ((['git', 'config', '--global', 'http.cookiefile'],),
       '/custom/.gitcookies'),
      (('os.path.exists', '/custom/.gitcookies'), False),
      (('ask_for_data', 'Reconfigure git to use default .gitcookies? '
                        'Press Enter to reconfigure, or Ctrl+C to abort'), ''),
      ((['git', 'config', '--global', 'http.cookiefile',
         os.path.expanduser('~/.gitcookies')], ), ''),
    ]
    self.assertEqual(0, git_cl.main(['creds-check']))
    self.assertRegexpMatches(
        sys.stdout.getvalue(),
        'WARNING: You have configured custom path to .gitcookies: ')
    self.assertRegexpMatches(
        sys.stdout.getvalue(),
        'However, your configured .gitcookies file is missing.')

  def test_git_cl_comment_add_gerrit(self):
    self.mock(git_cl.gerrit_util, 'SetReview',
              lambda host, change, msg, ready:
              self._mocked_call('SetReview', host, change, msg, ready))
    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), CERR1),
      ((['git', 'symbolic-ref', 'HEAD'],), CERR1),
      ((['git', 'config', 'rietveld.upstream-branch'],), CERR1),
      ((['git', 'branch', '-r'],), 'origin/HEAD -> origin/master\n'
                                   'origin/master'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/infra/infra'),
      (('SetReview', 'chromium-review.googlesource.com', 'infra%2Finfra~10',
        'msg', None),
       None),
    ]
    self.assertEqual(0, git_cl.main(['comment', '--gerrit', '-i', '10',
                                     '-a', 'msg']))

  def test_git_cl_comments_fetch_gerrit(self):
    self.mock(sys, 'stdout', StringIO.StringIO())
    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), CERR1),
      ((['git', 'symbolic-ref', 'HEAD'],), CERR1),
      ((['git', 'config', 'rietveld.upstream-branch'],), CERR1),
      ((['git', 'branch', '-r'],), 'origin/HEAD -> origin/master\n'
                                   'origin/master'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/infra/infra'),
      (('GetChangeDetail', 'chromium-review.googlesource.com',
        'infra%2Finfra~1',
        ['MESSAGES', 'DETAILED_ACCOUNTS']), {
        'owner': {'email': 'owner@example.com'},
        'messages': [
          {
             u'_revision_number': 1,
             u'author': {
               u'_account_id': 1111084,
               u'email': u'commit-bot@chromium.org',
               u'name': u'Commit Bot'
             },
             u'date': u'2017-03-15 20:08:45.000000000',
             u'id': u'f5a6c25ecbd3b3b54a43ae418ed97eff046dc50b',
             u'message': u'Patch Set 1:\n\nDry run: CQ is trying the patch...',
             u'tag': u'autogenerated:cq:dry-run'
          },
          {
             u'_revision_number': 2,
             u'author': {
               u'_account_id': 11151243,
               u'email': u'owner@example.com',
               u'name': u'owner'
             },
             u'date': u'2017-03-16 20:00:41.000000000',
             u'id': u'f5a6c25ecbd3b3b54a43ae418ed97eff046d1234',
             u'message': u'PTAL',
          },
          {
             u'_revision_number': 2,
             u'author': {
               u'_account_id': 148512 ,
               u'email': u'reviewer@example.com',
               u'name': u'reviewer'
             },
             u'date': u'2017-03-17 05:19:37.500000000',
             u'id': u'f5a6c25ecbd3b3b54a43ae418ed97eff046d4568',
             u'message': u'Patch Set 2: Code-Review+1',
          },
        ]
      }),
      (('GetChangeComments', 'chromium-review.googlesource.com',
        'infra%2Finfra~1'), {
        '/COMMIT_MSG': [
          {
            'author': {'email': u'reviewer@example.com'},
            'updated': u'2017-03-17 05:19:37.500000000',
            'patch_set': 2,
            'side': 'REVISION',
            'message': 'Please include a bug link',
          },
        ],
        'codereview.settings': [
          {
            'author': {'email': u'owner@example.com'},
            'updated': u'2017-03-16 20:00:41.000000000',
            'patch_set': 2,
            'side': 'PARENT',
            'line': 42,
            'message': 'I removed this because it is bad',
          },
        ]
      })
    ] * 2 + [
      (('write_json', 'output.json', [
        {
          u'date': u'2017-03-16 20:00:41.000000',
          u'message': (
              u'PTAL\n' +
              u'\n' +
              u'codereview.settings\n' +
              u'  Base, Line 42: https://chromium-review.googlesource.com/' +
              u'c/1/2/codereview.settings#b42\n' +
              u'  I removed this because it is bad\n'),
          u'approval': False,
          u'disapproval': False,
          u'sender': u'owner@example.com'
        }, {
          u'date': u'2017-03-17 05:19:37.500000',
          u'message': (
              u'Patch Set 2: Code-Review+1\n' +
              u'\n' +
              u'/COMMIT_MSG\n' +
              u'  PS2, File comment: https://chromium-review.googlesource' +
              u'.com/c/1/2//COMMIT_MSG#\n' +
              u'  Please include a bug link\n'),
          u'approval': False,
          u'disapproval': False,
          u'sender': u'reviewer@example.com'
        }
      ]),'')
    ]
    expected_comments_summary = [
      git_cl._CommentSummary(
        message=(
            u'PTAL\n' +
            u'\n' +
            u'codereview.settings\n' +
            u'  Base, Line 42: https://chromium-review.googlesource.com/' +
            u'c/1/2/codereview.settings#b42\n' +
            u'  I removed this because it is bad\n'),
        date=datetime.datetime(2017, 3, 16, 20, 0, 41, 0),
        disapproval=False, approval=False, sender=u'owner@example.com'),
      git_cl._CommentSummary(
        message=(
            u'Patch Set 2: Code-Review+1\n' +
            u'\n' +
            u'/COMMIT_MSG\n' +
            u'  PS2, File comment: https://chromium-review.googlesource.com/' +
            u'c/1/2//COMMIT_MSG#\n' +
            u'  Please include a bug link\n'),
        date=datetime.datetime(2017, 3, 17, 5, 19, 37, 500000),
        disapproval=False, approval=False, sender=u'reviewer@example.com'),
    ]
    cl = git_cl.Changelist(codereview='gerrit', issue=1)
    self.assertEqual(cl.GetCommentsSummary(), expected_comments_summary)
    self.assertEqual(0, git_cl.main(['comment', '-i', '1',
                                      '-j', 'output.json']))

  def test_get_remote_url_with_mirror(self):
    original_os_path_isdir = os.path.isdir
    def selective_os_path_isdir_mock(path):
      if path == '/cache/this-dir-exists':
        return self._mocked_call('os.path.isdir', path)
      return original_os_path_isdir(path)
    self.mock(os.path, 'isdir', selective_os_path_isdir_mock)

    url = 'https://chromium.googlesource.com/my/repo'
    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       '/cache/this-dir-exists'),
      (('os.path.isdir', '/cache/this-dir-exists'),
       True),
      # Runs in /cache/this-dir-exists.
      ((['git', 'config', 'remote.origin.url'],),
       url),
    ]
    cl = git_cl.Changelist(codereview='gerrit', issue=1)
    self.assertEqual(cl.GetRemoteUrl(), url)
    self.assertEqual(cl.GetRemoteUrl(), url)  # Must be cached.

  def test_gerrit_change_identifier_with_project(self):
    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],),
       'https://chromium.googlesource.com/a/my/repo.git/'),
    ]
    cl = git_cl.Changelist(codereview='gerrit', issue=123456)
    self.assertEqual(cl._GerritChangeIdentifier(), 'my%2Frepo~123456')

  def test_gerrit_change_identifier_without_project(self):
    self.calls = [
      ((['git', 'symbolic-ref', 'HEAD'],), 'master'),
      ((['git', 'config', 'branch.master.merge'],), 'master'),
      ((['git', 'config', 'branch.master.remote'],), 'origin'),
      ((['git', 'config', 'remote.origin.url'],), CERR1),
    ]
    cl = git_cl.Changelist(codereview='gerrit', issue=123456)
    self.assertEqual(cl._GerritChangeIdentifier(), '123456')


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
