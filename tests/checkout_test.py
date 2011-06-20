#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for checkout.py."""

from __future__ import with_statement
import logging
import os
import shutil
import sys
import unittest
from xml.etree import ElementTree

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DIR = os.path.join(ROOT_DIR, '..')
sys.path.insert(0, BASE_DIR)

import checkout
import patch
import subprocess2
from tests import fake_repos


# pass -v to enable it.
DEBUGGING = False

# A naked patch.
NAKED_PATCH = ("""\
--- svn_utils_test.txt
+++ svn_utils_test.txt
@@ -3,6 +3,7 @@ bb
 ccc
 dd
 e
+FOO!
 ff
 ggg
 hh
""")

# A patch generated from git.
GIT_PATCH = ("""\
diff --git a/svn_utils_test.txt b/svn_utils_test.txt
index 0e4de76..8320059 100644
--- a/svn_utils_test.txt
+++ b/svn_utils_test.txt
@@ -3,6 +3,7 @@ bb
 ccc
 dd
 e
+FOO!
 ff
 ggg
 hh
""")

# A patch that will fail to apply.
BAD_PATCH = ("""\
diff --git a/svn_utils_test.txt b/svn_utils_test.txt
index 0e4de76..8320059 100644
--- a/svn_utils_test.txt
+++ b/svn_utils_test.txt
@@ -3,7 +3,8 @@ bb
 ccc
 dd
+FOO!
 ff
 ggg
 hh
""")

PATCH_ADD = ("""\
diff --git a/new_dir/subdir/new_file b/new_dir/subdir/new_file
new file mode 100644
--- /dev/null
+++ b/new_dir/subdir/new_file
@@ -0,0 +1,2 @@
+A new file
+should exist.
""")


class FakeRepos(fake_repos.FakeReposBase):
  def populateSvn(self):
    """Creates a few revisions of changes files."""
    subprocess2.check_call(
        ['svn', 'checkout', self.svn_base, self.svn_checkout, '-q',
         '--non-interactive', '--no-auth-cache',
         '--username', self.USERS[0][0], '--password', self.USERS[0][1]])
    assert os.path.isdir(os.path.join(self.svn_checkout, '.svn'))
    fs = {}
    fs['trunk/origin'] = 'svn@1'
    fs['trunk/codereview.settings'] = (
        '# Test data\n'
        'bar: pouet\n')
    fs['trunk/svn_utils_test.txt'] = (
        'a\n'
        'bb\n'
        'ccc\n'
        'dd\n'
        'e\n'
        'ff\n'
        'ggg\n'
        'hh\n'
        'i\n'
        'jj\n'
        'kkk\n'
        'll\n'
        'm\n'
        'nn\n'
        'ooo\n'
        'pp\n'
        'q\n')
    self._commit_svn(fs)
    fs['trunk/origin'] = 'svn@2\n'
    fs['trunk/extra'] = 'dummy\n'
    fs['trunk/bin_file'] = '\x00'
    self._commit_svn(fs)

  def populateGit(self):
    raise NotImplementedError()


# pylint: disable=R0201
class BaseTest(fake_repos.FakeReposTestBase):
  name = 'foo'
  FAKE_REPOS_CLASS = FakeRepos

  def setUp(self):
    # Need to enforce subversion_config first.
    checkout.SvnMixIn.svn_config_dir = os.path.join(
        ROOT_DIR, 'subversion_config')
    super(BaseTest, self).setUp()
    self._old_call = subprocess2.call
    def redirect_call(args, **kwargs):
      if not DEBUGGING:
        kwargs.setdefault('stdout', subprocess2.PIPE)
        kwargs.setdefault('stderr', subprocess2.STDOUT)
      return self._old_call(args, **kwargs)
    subprocess2.call = redirect_call
    self.usr, self.pwd = self.FAKE_REPOS.USERS[0]
    self.previous_log = None

  def tearDown(self):
    subprocess2.call = self._old_call
    super(BaseTest, self).tearDown()

  def get_patches(self):
    return patch.PatchSet([
        patch.FilePatchDiff(
            'svn_utils_test.txt', GIT_PATCH, []),
        # TODO(maruel): Test with is_new == False.
        patch.FilePatchBinary('bin_file', '\x00', [], is_new=True),
        patch.FilePatchDelete('extra', False),
        patch.FilePatchDiff('new_dir/subdir/new_file', PATCH_ADD, []),
    ])

  def get_trunk(self, modified):
    tree = {}
    subroot = 'trunk/'
    for k, v in self.FAKE_REPOS.svn_revs[-1].iteritems():
      if k.startswith(subroot):
        f = k[len(subroot):]
        assert f not in tree
        tree[f] = v

    if modified:
      content_lines = tree['svn_utils_test.txt'].splitlines(True)
      tree['svn_utils_test.txt'] = ''.join(
          content_lines[0:5] + ['FOO!\n'] + content_lines[5:])
      del tree['extra']
      tree['new_dir/subdir/new_file'] = 'A new file\nshould exist.\n'
    return tree

  def _check_base(self, co, root, git, expected):
    read_only = isinstance(co, checkout.ReadOnlyCheckout)
    assert not read_only == bool(expected)
    if not read_only:
      self.FAKE_REPOS.svn_dirty = True

    self.assertEquals(root, co.project_path)
    self.assertEquals(self.previous_log['revision'], co.prepare(None))
    self.assertEquals('pouet', co.get_settings('bar'))
    self.assertTree(self.get_trunk(False), root)
    patches = self.get_patches()
    co.apply_patch(patches)
    self.assertEquals(
        ['bin_file', 'extra', 'new_dir/subdir/new_file', 'svn_utils_test.txt'],
        sorted(patches.filenames))

    if git:
      # Hackish to verify _branches() internal function.
      # pylint: disable=W0212
      self.assertEquals(
          (['master', 'working_branch'], 'working_branch'),
          co.checkout._branches())

    # Verify that the patch is applied even for read only checkout.
    self.assertTree(self.get_trunk(True), root)
    fake_author = self.FAKE_REPOS.USERS[1][0]
    revision = co.commit(u'msg', fake_author)
    # Nothing changed.
    self.assertTree(self.get_trunk(True), root)

    if read_only:
      self.assertEquals('FAKE', revision)
      self.assertEquals(self.previous_log['revision'], co.prepare(None))
      # Changes should be reverted now.
      self.assertTree(self.get_trunk(False), root)
      expected = self.previous_log
    else:
      self.assertEquals(self.previous_log['revision'] + 1, revision)
      self.assertEquals(self.previous_log['revision'] + 1, co.prepare(None))
      self.assertTree(self.get_trunk(True), root)
      expected = expected.copy()
      expected['msg'] = 'msg'
      expected['revision'] = self.previous_log['revision'] + 1
      expected.setdefault('author', fake_author)

    actual = self._log()
    self.assertEquals(expected, actual)

  def _check_exception(self, co, err_msg):
    co.prepare(None)
    try:
      co.apply_patch([patch.FilePatchDiff('svn_utils_test.txt', BAD_PATCH, [])])
      self.fail()
    except checkout.PatchApplicationFailed, e:
      self.assertEquals(e.filename, 'svn_utils_test.txt')
      self.assertEquals(e.status, err_msg)

  def _log(self):
    raise NotImplementedError()

  def _test_process(self, co_lambda):
    """Makes sure the process lambda is called correctly."""
    post_processors = [lambda *args: results.append(args)]
    co = co_lambda(post_processors)
    self.assertEquals(post_processors, co.post_processors)
    co.prepare(None)
    ps = self.get_patches()
    results = []
    co.apply_patch(ps)
    expected = [(co, p) for p in ps.patches]
    self.assertEquals(expected, results)


class SvnBaseTest(BaseTest):
  def setUp(self):
    super(SvnBaseTest, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_svn()
    self.assertTrue(self.enabled)
    self.svn_trunk = 'trunk'
    self.svn_url = self.svn_base + self.svn_trunk
    self.previous_log = self._log()

  def _log(self):
    # Don't use the local checkout in case of caching incorrency.
    out = subprocess2.check_output(
        ['svn', 'log', self.svn_url,
         '--non-interactive', '--no-auth-cache',
         '--username', self.usr, '--password', self.pwd,
         '--with-all-revprops', '--xml',
         '--limit', '1'])
    logentry = ElementTree.XML(out).find('logentry')
    if logentry == None:
      return {'revision': 0}
    data = {
        'revision': int(logentry.attrib['revision']),
    }
    def set_item(name):
      item = logentry.find(name)
      if item != None:
        data[name] = item.text
    set_item('author')
    set_item('msg')
    revprops = logentry.find('revprops')
    if revprops != None:
      data['revprops'] = []
      for prop in revprops.getiterator('property'):
        data['revprops'].append((prop.attrib['name'], prop.text))
    return data

  def _test_prepare(self, co):
    self.assertEquals(1, co.prepare(1))


class SvnCheckout(SvnBaseTest):
  def _get_co(self, read_only):
    if read_only:
      return checkout.ReadOnlyCheckout(
          checkout.SvnCheckout(
              self.root_dir, self.name, None, None, self.svn_url))
    else:
      return checkout.SvnCheckout(
          self.root_dir, self.name, self.usr, self.pwd, self.svn_url)

  def _check(self, read_only, expected):
    root = os.path.join(self.root_dir, self.name)
    self._check_base(self._get_co(read_only), root, False, expected)

  def testAllRW(self):
    expected = {
        'author': self.FAKE_REPOS.USERS[0][0],
        'revprops': [('realauthor', self.FAKE_REPOS.USERS[1][0])]
    }
    self._check(False, expected)

  def testAllRO(self):
    self._check(True, None)

  def testException(self):
    self._check_exception(
        self._get_co(True),
        'While running patch -p1 --forward --force;\n'
        'patching file svn_utils_test.txt\n'
        'Hunk #1 FAILED at 3.\n'
        '1 out of 1 hunk FAILED -- saving rejects to file '
        'svn_utils_test.txt.rej\n')

  def testSvnProps(self):
    co = self._get_co(False)
    co.prepare(None)
    try:
      # svn:ignore can only be applied to directories.
      svn_props = [('svn:ignore', 'foo')]
      co.apply_patch(
          [patch.FilePatchDiff('svn_utils_test.txt', NAKED_PATCH, svn_props)])
      self.fail()
    except checkout.PatchApplicationFailed, e:
      self.assertEquals(e.filename, 'svn_utils_test.txt')
      self.assertEquals(
          e.status,
          'While running svn propset svn:ignore foo svn_utils_test.txt '
          '--non-interactive;\n'
          'patching file svn_utils_test.txt\n'
          'svn: Cannot set \'svn:ignore\' on a file (\'svn_utils_test.txt\')\n')
    co.prepare(None)
    svn_props = [('svn:eol-style', 'LF'), ('foo', 'bar')]
    co.apply_patch(
        [patch.FilePatchDiff('svn_utils_test.txt', NAKED_PATCH, svn_props)])
    filepath = os.path.join(self.root_dir, self.name, 'svn_utils_test.txt')
    # Manually verify the properties.
    props = subprocess2.check_output(
        ['svn', 'proplist', filepath],
        cwd=self.root_dir).splitlines()[1:]
    props = sorted(p.strip() for p in props)
    expected_props = dict(svn_props)
    self.assertEquals(sorted(expected_props.iterkeys()), props)
    for k, v in expected_props.iteritems():
      value = subprocess2.check_output(
        ['svn', 'propget', '--strict', k, filepath],
        cwd=self.root_dir).strip()
      self.assertEquals(v, value)

  def testWithRevPropsSupport(self):
    # Add the hook that will commit in a way that removes the race condition.
    hook = os.path.join(self.FAKE_REPOS.svn_repo, 'hooks', 'pre-commit')
    shutil.copyfile(os.path.join(ROOT_DIR, 'sample_pre_commit_hook'), hook)
    os.chmod(hook, 0755)
    expected = {
        'revprops': [('commit-bot', 'user1@example.com')],
    }
    self._check(False, expected)

  def testWithRevPropsSupportNotCommitBot(self):
    # Add the hook that will commit in a way that removes the race condition.
    hook = os.path.join(self.FAKE_REPOS.svn_repo, 'hooks', 'pre-commit')
    shutil.copyfile(os.path.join(ROOT_DIR, 'sample_pre_commit_hook'), hook)
    os.chmod(hook, 0755)
    co = checkout.SvnCheckout(
        self.root_dir, self.name,
        self.FAKE_REPOS.USERS[1][0], self.FAKE_REPOS.USERS[1][1],
        self.svn_url)
    root = os.path.join(self.root_dir, self.name)
    expected = {
        'author': self.FAKE_REPOS.USERS[1][0],
    }
    self._check_base(co, root, False, expected)

  def testAutoProps(self):
    co = self._get_co(False)
    co.svn_config = checkout.SvnConfig(
        os.path.join(ROOT_DIR, 'subversion_config'))
    co.prepare(None)
    patches = self.get_patches()
    co.apply_patch(patches)
    self.assertEquals(
        ['bin_file', 'extra', 'new_dir/subdir/new_file', 'svn_utils_test.txt'],
        sorted(patches.filenames))
    # *.txt = svn:eol-style=LF in subversion_config/config.
    out = subprocess2.check_output(
        ['svn', 'pget', 'svn:eol-style', 'svn_utils_test.txt'],
        cwd=co.project_path)
    self.assertEquals('LF\n', out)

  def testProcess(self):
    co = lambda x: checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url,
        x)
    self._test_process(co)

  def testPrepare(self):
    co = checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url)
    self._test_prepare(co)


class GitSvnCheckout(SvnBaseTest):
  name = 'foo.git'

  def _get_co(self, read_only):
    co = checkout.GitSvnCheckout(
        self.root_dir, self.name[:-4],
        self.usr, self.pwd,
        self.svn_base, self.svn_trunk)
    if read_only:
      co = checkout.ReadOnlyCheckout(co)
    else:
      # Hack to simplify testing.
      co.checkout = co
    return co

  def _check(self, read_only, expected):
    root = os.path.join(self.root_dir, self.name)
    self._check_base(self._get_co(read_only), root, True, expected)

  def testAllRO(self):
    self._check(True, None)

  def testAllRW(self):
    expected = {
        'author': self.FAKE_REPOS.USERS[0][0],
    }
    self._check(False, expected)

  def testGitSvnPremade(self):
    # Test premade git-svn clone. First make a git-svn clone.
    git_svn_co = self._get_co(True)
    revision = git_svn_co.prepare(None)
    self.assertEquals(self.previous_log['revision'], revision)
    # Then use GitSvnClone to clone it to lose the git-svn connection and verify
    # git svn init / git svn fetch works.
    git_svn_clone = checkout.GitSvnPremadeCheckout(
        self.root_dir, self.name[:-4] + '2', 'trunk',
        self.usr, self.pwd,
        self.svn_base, self.svn_trunk, git_svn_co.project_path)
    self.assertEquals(
        self.previous_log['revision'], git_svn_clone.prepare(None))

  def testException(self):
    self._check_exception(
        self._get_co(True), 'fatal: corrupt patch at line 12\n')

  def testSvnProps(self):
    co = self._get_co(False)
    co.prepare(None)
    try:
      svn_props = [('foo', 'bar')]
      co.apply_patch(
          [patch.FilePatchDiff('svn_utils_test.txt', NAKED_PATCH, svn_props)])
      self.fail()
    except patch.UnsupportedPatchFormat, e:
      self.assertEquals(e.filename, 'svn_utils_test.txt')
      self.assertEquals(
          e.status,
          'Cannot apply svn property foo to file svn_utils_test.txt.')
    co.prepare(None)
    # svn:eol-style is ignored.
    svn_props = [('svn:eol-style', 'LF')]
    co.apply_patch(
        [patch.FilePatchDiff('svn_utils_test.txt', NAKED_PATCH, svn_props)])

  def testProcess(self):
    co = lambda x: checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url, x)
    self._test_process(co)

  def testPrepare(self):
    co = checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url)
    self._test_prepare(co)


class RawCheckout(SvnBaseTest):
  def setUp(self):
    super(RawCheckout, self).setUp()
    # Use a svn checkout as the base.
    self.base_co = checkout.SvnCheckout(
        self.root_dir, self.name, None, None, self.svn_url)
    self.base_co.prepare(None)

  def _get_co(self, read_only):
    co = checkout.RawCheckout(self.root_dir, self.name, None)
    if read_only:
      return checkout.ReadOnlyCheckout(co)
    return co

  def _check(self, read_only):
    root = os.path.join(self.root_dir, self.name)
    co = self._get_co(read_only)

    # A copy of BaseTest._check_base()
    self.assertEquals(root, co.project_path)
    self.assertEquals(None, co.prepare(None))
    self.assertEquals('pouet', co.get_settings('bar'))
    self.assertTree(self.get_trunk(False), root)
    patches = self.get_patches()
    co.apply_patch(patches)
    self.assertEquals(
        ['bin_file', 'extra', 'new_dir/subdir/new_file', 'svn_utils_test.txt'],
        sorted(patches.filenames))

    # Verify that the patch is applied even for read only checkout.
    self.assertTree(self.get_trunk(True), root)
    if read_only:
      revision = co.commit(u'msg', self.FAKE_REPOS.USERS[1][0])
      self.assertEquals('FAKE', revision)
    else:
      try:
        co.commit(u'msg', self.FAKE_REPOS.USERS[1][0])
        self.fail()
      except NotImplementedError:
        pass
    self.assertTree(self.get_trunk(True), root)
    # Verify that prepare() is a no-op.
    self.assertEquals(None, co.prepare(None))
    self.assertTree(self.get_trunk(True), root)

  def testAllRW(self):
    self._check(False)

  def testAllRO(self):
    self._check(True)

  def testException(self):
    self._check_exception(
        self._get_co(True),
        'patching file svn_utils_test.txt\n'
        'Hunk #1 FAILED at 3.\n'
        '1 out of 1 hunk FAILED -- saving rejects to file '
        'svn_utils_test.txt.rej\n')

  def testProcess(self):
    co = lambda x: checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url, x)
    self._test_process(co)

  def testPrepare(self):
    co = checkout.SvnCheckout(
        self.root_dir, self.name,
        None, None,
        self.svn_url)
    self._test_prepare(co)


if __name__ == '__main__':
  if '-v' in sys.argv:
    DEBUGGING = True
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  else:
    logging.basicConfig(
        level=logging.ERROR,
        format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  unittest.main()
