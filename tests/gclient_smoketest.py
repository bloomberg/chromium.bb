#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Smoke tests for gclient.py.

Shell out 'gclient' and run basic conformance tests.

This test assumes GClientSmokeBase.URL_BASE is valid.
"""

import logging
import os
import pprint
import re
import shutil
import subprocess
import sys
import unittest

from fake_repos import rmtree, write, FakeRepos

join = os.path.join

SHOULD_LEAK = False
UNITTEST_DIR = os.path.abspath(os.path.dirname(__file__))
GCLIENT_PATH = join(os.path.dirname(UNITTEST_DIR), 'gclient')
# all tests outputs goes there.
TRIAL_DIR = join(UNITTEST_DIR, '_trial')
# In case you want to use another machine to create the fake repos, e.g.
# not on Windows.
HOST = '127.0.0.1'
FAKE = None


def read_tree(tree_root):
  """Returns a dict of all the files in a tree."""
  tree = {}
  for root, dirs, files in os.walk(tree_root):
    for d in filter(lambda x: x.startswith('.'), dirs):
      dirs.remove(d)
    for f in [join(root, f) for f in files if not f.startswith('.')]:
      tree[f[len(tree_root) + 1:]] = open(join(root, f), 'rb').read()
  return tree


def dict_diff(dict1, dict2):
  diff = {}
  for k, v in dict1.iteritems():
    if k not in dict2:
      diff[k] = v
    elif v != dict2[k]:
      diff[k] = (v, dict2[k])
  for k, v in dict2.iteritems():
    if k not in dict1:
      diff[k] = v
  return diff


def mangle_svn_tree(*args):
  result = {}
  for old_root, new_root, tree in args:
    for k, v in tree.iteritems():
      if not k.startswith(old_root):
        continue
      result[join(new_root, k[len(old_root) + 1:])] = v
  return result


def mangle_git_tree(*args):
  result = {}
  for new_root, tree in args:
    for k, v in tree.iteritems():
      result[join(new_root, k)] = v
  return result


class GClientSmokeBase(unittest.TestCase):
  # This subversion repository contains a test repository.
  ROOT_DIR = join(TRIAL_DIR, 'smoke')

  def setUp(self):
    # Vaguely inspired by twisted.
    # Make sure it doesn't try to auto update when testing!
    self.env = os.environ.copy()
    self.env['DEPOT_TOOLS_UPDATE'] = '0'
    # Remove left overs
    self.root_dir = join(self.ROOT_DIR, self.id())
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

  def checkString(self, expected, result):
    if expected != result:
      # Strip the begining
      while expected and result and expected[0] == result[0]:
        expected = expected[1:]
        result = result[1:]
      # The exception trace makes it hard to read so dump it too.
      if '\n' in result:
        print result
    self.assertEquals(expected, result)

  def check(self, expected, results):
    self.checkString(expected[0], results[0])
    self.checkString(expected[1], results[1])
    self.assertEquals(expected[2], results[2])

  def assertTree(self, tree):
    actual = read_tree(self.root_dir)
    diff = dict_diff(tree, actual)
    if diff:
      logging.debug('Actual %s\n%s' % (self.root_dir, pprint.pformat(actual)))
      logging.debug('Expected\n%s' % pprint.pformat(tree))
      logging.debug('Diff\n%s' % pprint.pformat(diff))
    self.assertEquals(tree, actual)


class GClientSmoke(GClientSmokeBase):
  def testHelp(self):
    """testHelp: make sure no new command was added."""
    result = self.gclient(['help'])
    self.assertEquals(1197, len(result[0]))
    self.assertEquals(0, len(result[1]))
    self.assertEquals(0, result[2])

  def testUnknown(self):
    result = self.gclient(['foo'])
    self.assertEquals(1197, len(result[0]))
    self.assertEquals(0, len(result[1]))
    self.assertEquals(0, result[2])

  def testNotConfigured(self):
    res = ('', 'Error: client not configured; see \'gclient config\'\n', 1)
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

  def testConfig(self):
    p = join(self.root_dir, '.gclient')
    def test(cmd, expected):
      if os.path.exists(p):
        os.remove(p)
      results = self.gclient(cmd)
      self.check(('', '', 0), results)
      self.checkString(expected, open(p, 'rb').read())

    test(['config', self.svn_base + 'trunk/src/'],
         'solutions = [\n'
         '  { "name"        : "src",\n'
         '    "url"         : "svn://127.0.0.1/svn/trunk/src",\n'
         '    "custom_deps" : {\n'
         '    },\n'
         '    "safesync_url": ""\n'
         '  },\n]\n')

    test(['config', self.git_base + 'repo_1', '--name', 'src'],
         'solutions = [\n'
         '  { "name"        : "src",\n'
         '    "url"         : "git://127.0.0.1/git/repo_1",\n'
         '    "custom_deps" : {\n'
         '    },\n'
         '    "safesync_url": ""\n'
         '  },\n]\n')

    test(['config', 'foo', 'faa'],
         'solutions = [\n'
         '  { "name"        : "foo",\n'
         '    "url"         : "foo",\n'
         '    "custom_deps" : {\n'
         '    },\n'
         '    "safesync_url": "faa"\n'
         '  },\n]\n')

    test(['config', '--spec', '["blah blah"]'], '["blah blah"]')

    os.remove(p)
    results = self.gclient(['config', 'foo', 'faa', 'fuu'])
    err = ('Usage: gclient.py config [options] [url] [safesync url]\n\n'
           'gclient.py: error: Inconsistent arguments. Use either --spec or one'
           ' or 2 args\n')
    self.check(('', err, 2), results)
    self.assertFalse(os.path.exists(join(self.root_dir, '.gclient')))


class GClientSmokeSVN(GClientSmokeBase):
  def testSync(self):
    # TODO(maruel): safesync, multiple solutions, invalid@revisions,
    # multiple revisions.
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Test unversioned checkout.
    results = self.gclient(['sync', '--deps', 'mac'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(17, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_svn_tree(
        (join('trunk', 'src'), 'src', FAKE.svn_revs[-1]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'foo'),
            FAKE.svn_revs[1]),
        (join('trunk', 'other'), join('src', 'other'), FAKE.svn_revs[2]),
        )
    tree[join('src', 'hooked1')] = 'hooked1'
    self.assertTree(tree)

    # Manually remove hooked1 before synching to make sure it's not recreated.
    os.remove(join(self.root_dir, 'src', 'hooked1'))

    # Test incremental versioned sync: sync backward.
    results = self.gclient(['sync', '--revision', 'src@1', '--deps', 'mac',
                            '--delete_unversioned_trees'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(19, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_svn_tree(
        (join('trunk', 'src'), 'src', FAKE.svn_revs[1]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'fpp'),
            FAKE.svn_revs[2]),
        (join('trunk', 'other'), join('src', 'other'), FAKE.svn_revs[2]),
        (join('trunk', 'third_party', 'foo'),
            join('src', 'third_party', 'prout'),
            FAKE.svn_revs[2]),
        )
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    results = self.gclient(['sync', '--deps', 'mac'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(21, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_svn_tree(
        (join('trunk', 'src'), 'src', FAKE.svn_revs[-1]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'fpp'),
            FAKE.svn_revs[2]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'foo'),
            FAKE.svn_revs[1]),
        (join('trunk', 'other'), join('src', 'other'), FAKE.svn_revs[2]),
        (join('trunk', 'third_party', 'foo'),
            join('src', 'third_party', 'prout'),
            FAKE.svn_revs[2]),
        )
    tree[join('src', 'hooked1')] = 'hooked1'
    self.assertTree(tree)

  def testRevertAndStatus(self):
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac'])
    write(join(self.root_dir, 'src', 'other', 'hi'), 'Hey!')

    results = self.gclient(['status'])
    out = results[0].splitlines(False)
    self.assertEquals(out[0], '')
    self.assertTrue(out[1].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[2], '?       other')
    self.assertEquals(out[3], '?       hooked1')
    self.assertEquals(out[4], '?       third_party/foo')
    self.assertEquals(out[5], '')
    self.assertTrue(out[6].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[7], '?       hi')
    self.assertEquals(8, len(out))
    self.assertEquals('', results[1])
    self.assertEquals(0, results[2])

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert'])
    out = results[0].splitlines(False)
    self.assertEquals(22, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_svn_tree(
        (join('trunk', 'src'), 'src', FAKE.svn_revs[-1]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'foo'),
            FAKE.svn_revs[1]),
        (join('trunk', 'other'), join('src', 'other'), FAKE.svn_revs[2]),
        )
    tree[join('src', 'hooked1')] = 'hooked1'
    tree[join('src', 'hooked2')] = 'hooked2'
    self.assertTree(tree)

    results = self.gclient(['status'])
    out = results[0].splitlines(False)
    self.assertEquals(out[0], '')
    self.assertTrue(out[1].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[2], '?       other')
    self.assertEquals(out[3], '?       hooked1')
    self.assertEquals(out[4], '?       hooked2')
    self.assertEquals(out[5], '?       third_party/foo')
    self.assertEquals(6, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])

  def testRevertAndStatusDepsOs(self):
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac', '--revision', 'src@1'])
    write(join(self.root_dir, 'src', 'other', 'hi'), 'Hey!')

    results = self.gclient(['status', '--deps', 'mac'])
    out = results[0].splitlines(False)
    self.assertEquals(out[0], '')
    self.assertTrue(out[1].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[2], '?       other')
    self.assertEquals(out[3], '?       third_party/fpp')
    self.assertEquals(out[4], '?       third_party/prout')
    self.assertEquals(out[5], '')
    self.assertTrue(out[6].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[7], '?       hi')
    self.assertEquals(8, len(out))
    self.assertEquals('', results[1])
    self.assertEquals(0, results[2])

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert', '--deps', 'mac'])
    out = results[0].splitlines(False)
    self.assertEquals(24, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_svn_tree(
        (join('trunk', 'src'), 'src', FAKE.svn_revs[1]),
        (join('trunk', 'third_party', 'foo'), join('src', 'third_party', 'fpp'),
            FAKE.svn_revs[2]),
        (join('trunk', 'other'), join('src', 'other'), FAKE.svn_revs[2]),
        (join('trunk', 'third_party', 'prout'),
            join('src', 'third_party', 'prout'),
            FAKE.svn_revs[2]),
        )
    self.assertTree(tree)

    results = self.gclient(['status', '--deps', 'mac'])
    out = results[0].splitlines(False)
    self.assertEquals(out[0], '')
    self.assertTrue(out[1].startswith('________ running \'svn status\' in \''))
    self.assertEquals(out[2], '?       other')
    self.assertEquals(out[3], '?       third_party/fpp')
    self.assertEquals(out[4], '?       third_party/prout')
    self.assertEquals(5, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])

  def testRunHooks(self):
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['runhooks'])
    out = results[0].splitlines(False)
    self.assertEquals(4, len(out))
    self.assertEquals(out[0], '')
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked1\', \'w\'\)\.write\(\'hooked1\'\)\' in \'.*',
      out[1]))
    self.assertEquals(out[2], '')
    # runhooks runs all hooks even if not matching by design.
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked2\', \'w\'\)\.write\(\'hooked2\'\)\' in \'.*',
      out[3]))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])

  def testRunHooks(self):
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['runhooks'])
    out = results[0].splitlines(False)
    self.assertEquals(4, len(out))
    self.assertEquals(out[0], '')
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked1\', \'w\'\)\.write\(\'hooked1\'\)\' in \'.*',
      out[1]))
    self.assertEquals(out[2], '')
    # runhooks runs all hooks even if not matching by design.
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked2\', \'w\'\)\.write\(\'hooked2\'\)\' in \'.*',
      out[3]))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])

  def testRunHooksDepsOs(self):
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac', '--revision', 'src@1'])
    results = self.gclient(['runhooks'])
    self.check(('', '', 0), results)

  def testRevInfo(self):
    # TODO(maruel): Test multiple solutions.
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo'])
    out = ('src: %(base)s/src@2;\n'
           'src/other: %(base)s/other@2;\n'
           'src/third_party/foo: %(base)s/third_party/foo@1\n' %
          { 'base': self.svn_base + 'trunk' })
    self.check((out, '', 0), results)


class GClientSmokeGIT(GClientSmokeBase):
  def testSync(self):
    # TODO(maruel): safesync, multiple solutions, invalid@revisions,
    # multiple revisions.
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    # Test unversioned checkout.
    results = self.gclient(['sync', '--deps', 'mac'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3582 hooks run even if not matching, must
    # add sync parsing to get the list of updated files.
    self.assertEquals(13, len(out))
    self.assertTrue(results[1].startswith('Switched to a new branch \''))
    self.assertEquals(0, results[2])
    tree = mangle_git_tree(
        ('src', FAKE.git_hashes['repo_1'][1][1]),
        (join('src', 'repo2'), FAKE.git_hashes['repo_2'][0][1]),
        (join('src', 'repo2', 'repo_renamed'), FAKE.git_hashes['repo_3'][1][1]),
        )
    tree[join('src', 'hooked1')] = 'hooked1'
    tree[join('src', 'hooked2')] = 'hooked2'
    self.assertTree(tree)

    # Manually remove hooked1 before synching to make sure it's not recreated.
    os.remove(join(self.root_dir, 'src', 'hooked1'))

    # Test incremental versioned sync: sync backward.
    results = self.gclient(['sync', '--revision',
                            'src@' + FAKE.git_hashes['repo_1'][0][0],
                            '--deps', 'mac', '--delete_unversioned_trees'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(20, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_git_tree(
        ('src', FAKE.git_hashes['repo_1'][0][1]),
        (join('src', 'repo2'), FAKE.git_hashes['repo_2'][1][1]),
        (join('src', 'repo2', 'repo3'), FAKE.git_hashes['repo_3'][1][1]),
        (join('src', 'repo4'), FAKE.git_hashes['repo_4'][1][1]),
        )
    tree[join('src', 'hooked2')] = 'hooked2'
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    results = self.gclient(['sync', '--deps', 'mac'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(25, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_git_tree(
        ('src', FAKE.git_hashes['repo_1'][1][1]),
        (join('src', 'repo2'), FAKE.git_hashes['repo_2'][1][1]),
        (join('src', 'repo2', 'repo3'), FAKE.git_hashes['repo_3'][1][1]),
        (join('src', 'repo2', 'repo_renamed'), FAKE.git_hashes['repo_3'][1][1]),
        (join('src', 'repo4'), FAKE.git_hashes['repo_4'][1][1]),
        )
    tree[join('src', 'hooked1')] = 'hooked1'
    tree[join('src', 'hooked2')] = 'hooked2'
    self.assertTree(tree)

  def testRevertAndStatus(self):
    """TODO(maruel): Remove this line once this test is fixed."""
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac'])
    write(join(self.root_dir, 'src', 'repo2', 'hi'), 'Hey!')

    results = self.gclient(['status'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3584 It should output the unversioned
    # files.
    self.assertEquals(0, len(out))

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3583 It just runs the hooks right now.
    self.assertEquals(7, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = mangle_git_tree(
        ('src', FAKE.git_hashes['repo_1'][1][1]),
        (join('src', 'repo2'), FAKE.git_hashes['repo_2'][0][1]),
        (join('src', 'repo2', 'repo_renamed'), FAKE.git_hashes['repo_3'][1][1]),
        )
    # TODO(maruel): http://crosbug.com/3583 This file should have been removed.
    tree[join('src', 'repo2', 'hi')] = 'Hey!'
    tree[join('src', 'hooked1')] = 'hooked1'
    tree[join('src', 'hooked2')] = 'hooked2'
    self.assertTree(tree)

    results = self.gclient(['status'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3584 It should output the unversioned
    # files.
    self.assertEquals(0, len(out))

  def testRunHooks(self):
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['runhooks'])
    logging.debug(results[0])
    out = results[0].splitlines(False)
    self.assertEquals(4, len(out))
    self.assertEquals(out[0], '')
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked1\', \'w\'\)\.write\(\'hooked1\'\)\' in \'.*',
    out[1]))
    self.assertEquals(out[2], '')
    # runhooks runs all hooks even if not matching by design.
    self.assertTrue(re.match(r'^________ running \'.*?python -c '
      r'open\(\'src/hooked2\', \'w\'\)\.write\(\'hooked2\'\)\' in \'.*',
      out[3]))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])

  def testRevInfo(self):
    # TODO(maruel): Test multiple solutions.
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo'])
    out = ('src: %(base)srepo_1@%(hash1)s;\n'
           'src/repo2: %(base)srepo_2@%(hash2)s;\n'
           'src/repo2/repo_renamed: %(base)srepo_3@%(hash3)s\n' %
          {
            'base': self.git_base,
            'hash1': FAKE.git_hashes['repo_1'][1][0],
            'hash2': FAKE.git_hashes['repo_2'][0][0],
            'hash3': FAKE.git_hashes['repo_3'][1][0],
          })
    self.check((out, '', 0), results)


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
  if '-l' in sys.argv:
    SHOULD_LEAK = True
    sys.argv.remove('-l')
  FAKE = FakeRepos(TRIAL_DIR, SHOULD_LEAK, True)
  try:
    FAKE.setUp()
    unittest.main()
  finally:
    FAKE.tearDown()
