# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from dashboard.pinpoint.models.change import commit
from dashboard.pinpoint import test


def Commit(number, repository='chromium'):
  return commit.Commit(repository, 'commit ' + str(number))


class CommitTest(test.TestCase):

  def testCommit(self):
    c = commit.Commit('chromium', 'aaa7336c821888839f759c6c0a36')

    other_commit = commit.Commit(u'chromium', u'aaa7336c821888839f759c6c0a36')
    self.assertEqual(c, other_commit)
    self.assertEqual(str(c), 'chromium@aaa7336')
    self.assertEqual(c.id_string, 'chromium@aaa7336c821888839f759c6c0a36')
    self.assertEqual(c.repository, 'chromium')
    self.assertEqual(c.git_hash, 'aaa7336c821888839f759c6c0a36')
    self.assertEqual(c.repository_url, test.CHROMIUM_URL)

  def testDeps(self):
    self.file_contents.return_value = """
vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'webrtc_git': 'https://webrtc.googlesource.com',
  'webrtc_rev': 'deadbeef',
}
deps = {
  'src/v8': Var('chromium_git') + '/v8/v8.git' + '@' + 'c092edb',
  'src/third_party/lighttpd': {
    'url': Var('chromium_git') + '/deps/lighttpd.git' + '@' + '9dfa55d',
    'condition': 'checkout_mac or checkout_win',
  },
  'src/third_party/webrtc': {
    'url': '{webrtc_git}/src.git',
    'revision': '{webrtc_rev}',
  },
  'src/third_party/intellij': {
    'packages': [{
      'package': 'chromium/third_party/intellij',
      'version': 'version:12.0-cr0',
    }],
    'condition': 'checkout_android',
    'dep_type': 'cipd',
  },
}
deps_os = {
  'win': {
    'src/third_party/cygwin':
      Var('chromium_git') + '/chromium/deps/cygwin.git' + '@' + 'c89e446',
  }
}
    """

    expected = frozenset((
        ('https://chromium.googlesource.com/chromium/deps/cygwin', 'c89e446'),
        ('https://chromium.googlesource.com/deps/lighttpd', '9dfa55d'),
        ('https://chromium.googlesource.com/v8/v8', 'c092edb'),
        ('https://webrtc.googlesource.com/src', 'deadbeef'),
    ))
    self.assertEqual(Commit(0).Deps(), expected)

  def testAsDict(self):
    self.commit_info.side_effect = None
    self.commit_info.return_value = {
        'author': {'email': 'author@chromium.org'},
        'commit': 'aaa7336',
        'committer': {'time': 'Fri Jan 01 00:01:00 2016'},
        'message': 'Subject.\n\n'
                   'Commit message.\n'
                   'Cr-Commit-Position: refs/heads/master@{#437745}',
    }

    expected = {
        'repository': 'chromium',
        'git_hash': 'aaa7336',
        'url': test.CHROMIUM_URL + '/+/aaa7336',
        'subject': 'Subject.',
        'author': 'author@chromium.org',
        'time': 'Fri Jan 01 00:01:00 2016',
        'commit_position': 437745,
    }
    self.assertEqual(Commit(0).AsDict(), expected)

  def testFromDepNewRepo(self):
    dep = commit.Dep('https://new/repository/url.git', 'git_hash')
    c = commit.Commit.FromDep(dep)
    self.assertEqual(c, commit.Commit('url', 'git_hash'))

  def testFromDepExistingRepo(self):
    c = commit.Commit.FromDep(commit.Dep(test.CHROMIUM_URL, 'commit 0'))
    self.assertEqual(c, Commit(0))

  def testFromDict(self):
    c = commit.Commit.FromDict({
        'repository': 'chromium',
        'git_hash': 'commit 0',
    })
    self.assertEqual(c, Commit(0))

  def testFromDictWithRepositoryUrl(self):
    c = commit.Commit.FromDict({
        'repository': test.CHROMIUM_URL,
        'git_hash': 'commit 0',
    })
    self.assertEqual(c, Commit(0))

  def testFromDictResolvesHEAD(self):
    c = commit.Commit.FromDict({
        'repository': test.CHROMIUM_URL,
        'git_hash': 'HEAD',
    })

    expected = commit.Commit('chromium', 'git hash at HEAD')
    self.assertEqual(c, expected)

  def testFromDictFailureFromUnknownRepo(self):
    with self.assertRaises(KeyError):
      commit.Commit.FromDict({
          'repository': 'unknown repo',
          'git_hash': 'git hash',
      })

  def testFromDictFailureFromUnknownCommit(self):
    self.commit_info.side_effect = KeyError()

    with self.assertRaises(KeyError):
      commit.Commit.FromDict({
          'repository': 'chromium',
          'git_hash': 'unknown git hash',
      })


class MidpointTest(test.TestCase):

  def testSuccess(self):
    midpoint = commit.Commit.Midpoint(Commit(1), Commit(9))
    self.assertEqual(midpoint, Commit(5))

  def testSameCommit(self):
    midpoint = commit.Commit.Midpoint(Commit(1), Commit(1))
    self.assertEqual(midpoint, Commit(1))

  def testAdjacentCommits(self):
    midpoint = commit.Commit.Midpoint(Commit(1), Commit(2))
    self.assertEqual(midpoint, Commit(1))

  def testRaisesWithDifferingRepositories(self):
    with self.assertRaises(commit.NonLinearError):
      commit.Commit.Midpoint(Commit(1), Commit(2, repository='catapult'))

  def testRaisesWithEmptyRange(self):
    self.commit_range.side_effect = None
    self.commit_range.return_value = []

    with self.assertRaises(commit.NonLinearError):
      commit.Commit.Midpoint(Commit(1), Commit(9))
