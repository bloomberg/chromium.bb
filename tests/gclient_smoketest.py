#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Smoke tests for gclient.py.

Shell out 'gclient' and run basic conformance tests.

This test assumes GClientSmokeBase.URL_BASE is valid.
"""

# pylint: disable=E1103,W0403

import logging
import os
import re
import subprocess
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(ROOT_DIR))
from tests.fake_repos import join, write, FakeReposTestBase
import subprocess2

GCLIENT_PATH = os.path.join(os.path.dirname(ROOT_DIR), 'gclient')
COVERAGE = False


class GClientSmokeBase(FakeReposTestBase):
  def setUp(self):
    super(GClientSmokeBase, self).setUp()
    # Make sure it doesn't try to auto update when testing!
    self.env = os.environ.copy()
    self.env['DEPOT_TOOLS_UPDATE'] = '0'

  def gclient(self, cmd, cwd=None):
    if not cwd:
      cwd = self.root_dir
    if COVERAGE:
      # Don't use the wrapper script.
      cmd_base = ['coverage', 'run', '-a', GCLIENT_PATH + '.py']
    else:
      cmd_base = [GCLIENT_PATH]
    cmd = cmd_base + cmd
    process = subprocess.Popen(cmd, cwd=cwd, env=self.env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               shell=sys.platform.startswith('win'))
    (stdout, stderr) = process.communicate()
    logging.debug("XXX: %s\n%s\nXXX" % (' '.join(cmd), stdout))
    logging.debug("YYY: %s\n%s\nYYY" % (' '.join(cmd), stderr))
    return (stdout.replace('\r\n', '\n'), stderr.replace('\r\n', '\n'),
            process.returncode)

  def parseGclient(self, cmd, items, expected_stderr='', untangle=False):
    """Parse gclient's output to make it easier to test.
    If untangle is True, tries to sort out the output from parallel checkout."""
    (stdout, stderr, returncode) = self.gclient(cmd)
    if untangle:
      tasks = {}
      remaining = []
      for line in stdout.splitlines(False):
        m = re.match(r'^(\d)+>(.*)$', line)
        if not m:
          remaining.append(line)
        else:
          self.assertEquals([], remaining)
          tasks.setdefault(int(m.group(1)), []).append(m.group(2))
      out = []
      for key in sorted(tasks.iterkeys()):
        out.extend(tasks[key])
      out.extend(remaining)
      stdout = '\n'.join(out)
    self.checkString(expected_stderr, stderr)
    self.assertEquals(0, returncode)
    return self.checkBlock(stdout, items)

  def splitBlock(self, stdout):
    """Split gclient's output into logical execution blocks.
    ___ running 'foo' at '/bar'
    (...)
    ___ running 'baz' at '/bar'
    (...)

    will result in 2 items of len((...).splitlines()) each.
    """
    results = []
    for line in stdout.splitlines(False):
      # Intentionally skips empty lines.
      if not line:
        continue
      if line.startswith('__'):
        match = re.match(r'^________ ([a-z]+) \'(.*)\' in \'(.*)\'$', line)
        if not match:
          match = re.match(r'^_____ (.*) is missing, synching instead$', line)
          if match:
            # Blah, it's when a dependency is deleted, we should probably not
            # output this message.
            results.append([line])
          elif (
              not re.match(
                  r'_____ [^ ]+ : Attempting rebase onto [0-9a-f]+...',
                  line) and
              not re.match(r'_____ [^ ]+ at [^ ]+', line)):
            # The two regexp above are a bit too broad, they are necessary only
            # for git checkouts.
            self.fail(line)
        else:
          results.append([[match.group(1), match.group(2), match.group(3)]])
      else:
        if not results:
          # TODO(maruel): gclient's git stdout is inconsistent.
          # This should fail the test instead!!
          pass
        else:
          results[-1].append(line)
    return results

  def checkBlock(self, stdout, items):
    results = self.splitBlock(stdout)
    for i in xrange(min(len(results), len(items))):
      if isinstance(items[i], (list, tuple)):
        verb = items[i][0]
        path = items[i][1]
      else:
        verb = items[i]
        path = self.root_dir
      self.checkString(results[i][0][0], verb, (i, results[i][0][0], verb))
      if sys.platform == 'win32':
        # Make path lower case since casing can change randomly.
        self.checkString(
            results[i][0][2].lower(),
            path.lower(),
            (i, results[i][0][2].lower(), path.lower()))
      else:
        self.checkString(results[i][0][2], path, (i, results[i][0][2], path))
    self.assertEquals(len(results), len(items), (stdout, items, len(results)))
    return results

  @staticmethod
  def svnBlockCleanup(out):
    """Work around svn status difference between svn 1.5 and svn 1.6
    I don't know why but on Windows they are reversed. So sorts the items."""
    for i in xrange(len(out)):
      if len(out[i]) < 2:
        continue
      out[i] = [out[i][0]] + sorted([x[1:].strip() for x in out[i][1:]])
    return out


class GClientSmoke(GClientSmokeBase):
  """Doesn't require either svnserve nor git-daemon."""
  @property
  def svn_base(self):
    return 'svn://random.server/svn/'

  @property
  def git_base(self):
    return 'git://random.server/git/'

  def testHelp(self):
    """testHelp: make sure no new command was added."""
    result = self.gclient(['help'])
    # Roughly, not too short, not too long.
    self.assertTrue(1000 < len(result[0]) and len(result[0]) < 2000)
    self.assertEquals(0, len(result[1]))
    self.assertEquals(0, result[2])

  def testUnknown(self):
    result = self.gclient(['foo'])
    # Roughly, not too short, not too long.
    self.assertTrue(1000 < len(result[0]) and len(result[0]) < 2000)
    self.assertEquals(0, len(result[1]))
    self.assertEquals(0, result[2])

  def testNotConfigured(self):
    res = ('', 'Error: client not configured; see \'gclient config\'\n', 1)
    self.check(res, self.gclient(['cleanup']))
    self.check(res, self.gclient(['diff']))
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
      self.checkString(expected, open(p, 'rU').read())

    test(['config', self.svn_base + 'trunk/src/'],
         ('solutions = [\n'
          '  { "name"        : "src",\n'
          '    "url"         : "%strunk/src",\n'
          '    "deps_file"   : "DEPS",\n'
          '    "managed"     : True,\n'
          '    "custom_deps" : {\n'
          '    },\n'
          '    "safesync_url": "",\n'
          '  },\n'
          ']\n') % self.svn_base)

    test(['config', self.git_base + 'repo_1', '--name', 'src'],
         ('solutions = [\n'
          '  { "name"        : "src",\n'
          '    "url"         : "%srepo_1",\n'
          '    "deps_file"   : "DEPS",\n'
          '    "managed"     : True,\n'
          '    "custom_deps" : {\n'
          '    },\n'
          '    "safesync_url": "",\n'
          '  },\n'
          ']\n') % self.git_base)

    test(['config', 'foo', 'faa'],
         'solutions = [\n'
         '  { "name"        : "foo",\n'
         '    "url"         : "foo",\n'
         '    "deps_file"   : "DEPS",\n'
          '    "managed"     : True,\n'
         '    "custom_deps" : {\n'
         '    },\n'
         '    "safesync_url": "faa",\n'
         '  },\n'
         ']\n')

    test(['config', 'foo', '--deps', 'blah'],
         'solutions = [\n'
         '  { "name"        : "foo",\n'
         '    "url"         : "foo",\n'
         '    "deps_file"   : "blah",\n'
         '    "managed"     : True,\n'
         '    "custom_deps" : {\n'
         '    },\n'
          '    "safesync_url": "",\n'
         '  },\n'
         ']\n')

    test(['config', '--spec', '["blah blah"]'], '["blah blah"]')

    os.remove(p)
    results = self.gclient(['config', 'foo', 'faa', 'fuu'])
    err = ('Usage: gclient.py config [options] [url] [safesync url]\n\n'
           'gclient.py: error: Inconsistent arguments. Use either --spec or one'
           ' or 2 args\n')
    self.check(('', err, 2), results)
    self.assertFalse(os.path.exists(join(self.root_dir, '.gclient')))

  def testSolutionNone(self):
    results = self.gclient(['config', '--spec',
                            'solutions=[{"name": "./", "url": None}]'])
    self.check(('', '', 0), results)
    results = self.gclient(['sync'])
    self.check(('', '', 0), results)
    self.assertTree({})
    results = self.gclient(['revinfo'])
    self.check(('./: None\n', '', 0), results)
    self.check(('', '', 0), self.gclient(['cleanup']))
    self.check(('', '', 0), self.gclient(['diff']))
    self.assertTree({})
    self.check(('', '', 0), self.gclient(['pack']))
    self.check(('', '', 0), self.gclient(['revert']))
    self.assertTree({})
    self.check(('', '', 0), self.gclient(['runhooks']))
    self.assertTree({})
    self.check(('', '', 0), self.gclient(['status']))

  def testDifferentTopLevelDirectory(self):
    # Check that even if the .gclient file does not mention the directory src
    # itself, but it is included via dependencies, the .gclient file is used.
    self.gclient(['config', self.svn_base + 'trunk/src.DEPS'])
    deps = join(self.root_dir, 'src.DEPS')
    os.mkdir(deps)
    write(join(deps, 'DEPS'),
        'deps = { "src": "%strunk/src" }' % (self.svn_base))
    src = join(self.root_dir, 'src')
    os.mkdir(src)
    res = self.gclient(['status', '--jobs', '1'], src)
    self.checkBlock(res[0], [('running', deps), ('running', src)])


class GClientSmokeSVN(GClientSmokeBase):
  def setUp(self):
    super(GClientSmokeSVN, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_svn()

  def testSync(self):
    # TODO(maruel): safesync.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Test unversioned checkout.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running',
          # This is due to the way svn update is called for a
          # single file when File() is used in a DEPS file.
          ('running', os.path.join(self.root_dir, 'src', 'file', 'other')),
          'running', 'running', 'running', 'running'])
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

    # Manually remove svn_hooked1 before synching to make sure it's not
    # recreated.
    os.remove(join(self.root_dir, 'src', 'svn_hooked1'))

    # Test incremental versioned sync: sync backward.
    self.parseGclient(
        ['sync', '--revision', 'src@1', '--deps', 'mac',
          '--delete_unversioned_trees', '--jobs', '1'],
        ['running', 'running', 'running', 'running', 'deleting'])
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
                      ['running', 'running', 'running', 'running', 'running'])
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testSyncTransitive(self):
    # TODO(maruel): safesync.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])

    # Make sure we can populate a new repository with --transitive.
    self.parseGclient(
        ['sync', '--transitive', '--revision', 'src@1', '--deps', 'mac',
          '--jobs', '1'],
        ['running', 'running', 'running', 'running'])
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@1', 'src/third_party/prout'))

    # Get up to date, so we can test synching back.
    self.gclient(['sync', '--deps', 'mac', '--jobs', '1'])

    # Manually remove svn_hooked1 before synching to make sure it's not
    # recreated.
    os.remove(join(self.root_dir, 'src', 'svn_hooked1'))

    self.parseGclient(
        ['sync', '--transitive', '--revision', 'src@1', '--deps', 'mac',
          '--delete_unversioned_trees', '--jobs', '1'],
        ['running', 'running', 'running', 'running', 'deleting'])
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@1', 'src/third_party/prout'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    self.assertTree(tree)

  def testSyncIgnoredSolutionName(self):
    """TODO(maruel): This will become an error soon."""
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    results = self.gclient(
        ['sync', '--deps', 'mac', '-r', 'invalid@1', '--jobs', '1'])
    self.checkBlock(results[0], [
        'running', 'running',
        # This is due to the way svn update is called for a single file when
        # File() is used in a DEPS file.
        ('running', os.path.join(self.root_dir, 'src', 'file', 'other')),
        'running', 'running', 'running', 'running'])
    self.checkString('Please fix your script, having invalid --revision flags '
        'will soon considered an error.\n', results[1])
    self.assertEquals(0, results[2])
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testSyncNoSolutionName(self):
    # When no solution name is provided, gclient uses the first solution listed.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.parseGclient(['sync', '--deps', 'mac', '-r', '1', '--jobs', '1'],
                      ['running', 'running', 'running', 'running'])
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout'))
    self.assertTree(tree)

  def testSyncJobs(self):
    if not self.enabled:
      return
    # TODO(maruel): safesync.
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Test unversioned checkout.
    self.parseGclient(
        ['sync', '--deps', 'mac', '--jobs', '8'],
        ['running', 'running',
        # This is due to the way svn update is called for a
        # single file when File() is used in a DEPS file.
        ('running', os.path.join(self.root_dir, 'src', 'file', 'other')),
        'running', 'running', 'running', 'running'],
        untangle=True)
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

    # Manually remove svn_hooked1 before synching to make sure it's not
    # recreated.
    os.remove(join(self.root_dir, 'src', 'svn_hooked1'))

    # Test incremental versioned sync: sync backward.
    self.parseGclient(
        ['sync', '--revision', 'src@1', '--deps', 'mac',
          '--delete_unversioned_trees', '--jobs', '8'],
        ['running', 'running', 'running', 'running', 'deleting'],
        untangle=True)
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '8'],
                      ['running', 'running', 'running', 'running', 'running'],
                      untangle=True)
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testSyncCustomDeps(self):
    if not self.enabled:
      return
    out = (
        'solutions = [\n'
        '  { "name"        : "src",\n'
        '    "url"         : "%(base)s/src",\n'
        '    "custom_deps" : {\n'
        # Remove 2 deps, change 1, add 1.
        '      "src/other": None,\n'
        '      "src/third_party/foo": \'%(base)s/third_party/prout\',\n'
        '      "src/file/other": None,\n'
        '      "new_deps": "/trunk/src/third_party",\n'
        '    },\n'
        '    "safesync_url": "",\n'
        '  },\n'
        ']\n\n' %
      { 'base': self.svn_base + 'trunk' })
    fileobj = open(os.path.join(self.root_dir, '.gclient'), 'w')
    fileobj.write(out)
    fileobj.close()
    self.parseGclient(
        ['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running', 'running', 'running'],
        untangle=True)
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/prout@2', 'src/third_party/foo'),
        ('trunk/src/third_party@2', 'new_deps'))
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testSyncCustomDepsNoDeps(self):
    if not self.enabled:
      return
    out = (
        'solutions = [\n'
        # This directory has no DEPS file.
        '  { "name"        : "src/third_party",\n'
        '    "url"         : "%(base)s/src/third_party",\n'
        '    "custom_deps" : {\n'
        # Add 1.
        '      "src/other": \'/trunk/other\',\n'
        '    },\n'
        '    "safesync_url": "",\n'
        '  },\n'
        ']\n\n' %
      { 'base': self.svn_base + 'trunk' })
    fileobj = open(os.path.join(self.root_dir, '.gclient'), 'w')
    fileobj.write(out)
    fileobj.close()
    self.parseGclient(
        ['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running'],
        untangle=True)
    tree = self.mangle_svn_tree(
        ('trunk/src/third_party@2', 'src/third_party'),
        ('trunk/other@2', 'src/other'))
    self.assertTree(tree)

  def testRevertAndStatus(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac'])
    write(join(self.root_dir, 'src', 'other', 'hi'), 'Hey!')

    out = self.parseGclient(['status', '--deps', 'mac', '--jobs', '1'],
                            [['running', join(self.root_dir, 'src')],
                             ['running', join(self.root_dir, 'src', 'other')]])
    out = self.svnBlockCleanup(out)
    self.checkString('file', out[0][1])
    self.checkString('other', out[0][2])
    self.checkString('svn_hooked1', out[0][3])
    self.checkString(join('third_party', 'foo'), out[0][4])
    self.checkString('hi', out[1][1])
    self.assertEquals(5, len(out[0]))
    self.assertEquals(2, len(out[1]))

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert', '--deps', 'mac', '--jobs', '1'])
    out = self.splitBlock(results[0])
    # src, src/other is missing, src/other, src/third_party/foo is missing,
    # src/third_party/foo, 2 svn hooks, 3 related to File().
    self.assertEquals(10, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other'))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/svn_hooked1'] = 'svn_hooked1'
    tree['src/svn_hooked2'] = 'svn_hooked2'
    self.assertTree(tree)

    out = self.parseGclient(['status', '--deps', 'mac', '--jobs', '1'],
                            [['running', join(self.root_dir, 'src')]])
    out = self.svnBlockCleanup(out)
    self.checkString('file', out[0][1])
    self.checkString('other', out[0][2])
    self.checkString('svn_hooked1', out[0][3])
    self.checkString('svn_hooked2', out[0][4])
    self.checkString(join('third_party', 'foo'), out[0][5])
    self.assertEquals(6, len(out[0]))
    self.assertEquals(1, len(out))

  def testRevertAndStatusDepsOs(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac', '--revision', 'src@1'])
    write(join(self.root_dir, 'src', 'other', 'hi'), 'Hey!')

    # Without --verbose, gclient won't output the directories without
    # modification.
    out = self.parseGclient(['status', '--deps', 'mac', '--jobs', '1'],
                            [['running', join(self.root_dir, 'src')],
                             ['running', join(self.root_dir, 'src', 'other')]])
    out = self.svnBlockCleanup(out)
    self.checkString('other', out[0][1])
    self.checkString(join('third_party', 'fpp'), out[0][2])
    self.checkString(join('third_party', 'prout'), out[0][3])
    self.checkString('hi', out[1][1])
    self.assertEquals(4, len(out[0]))
    self.assertEquals(2, len(out[1]))

    # So verify it works with --verbose.
    out = self.parseGclient(
        ['status', '--deps', 'mac', '--verbose', '--jobs', '1'],
        [['running', join(self.root_dir, 'src')],
          ['running', join(self.root_dir, 'src', 'third_party', 'fpp')],
          ['running', join(self.root_dir, 'src', 'other')],
          ['running', join(self.root_dir, 'src', 'third_party', 'prout')]])
    out = self.svnBlockCleanup(out)
    self.checkString('other', out[0][1])
    self.checkString(join('third_party', 'fpp'), out[0][2])
    self.checkString(join('third_party', 'prout'), out[0][3])
    self.checkString('hi', out[2][1])
    self.assertEquals(4, len(out[0]))
    self.assertEquals(1, len(out[1]))
    self.assertEquals(2, len(out[2]))
    self.assertEquals(1, len(out[3]))
    self.assertEquals(4, len(out))

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    # TODO(maruel): In general, gclient revert output is wrong. It should output
    # the file list after some ___ running 'svn status'
    results = self.gclient(['revert', '--deps', 'mac', '--jobs', '1'])
    out = self.splitBlock(results[0])
    self.assertEquals(7, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/prout@2', 'src/third_party/prout'))
    self.assertTree(tree)

    out = self.parseGclient(['status', '--deps', 'mac', '--jobs', '1'],
                            [['running', join(self.root_dir, 'src')]])
    out = self.svnBlockCleanup(out)
    self.checkString('other', out[0][1])
    self.checkString(join('third_party', 'fpp'), out[0][2])
    self.checkString(join('third_party', 'prout'), out[0][3])
    self.assertEquals(4, len(out[0]))

  def testRunHooks(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac'])
    out = self.parseGclient(['runhooks', '--deps', 'mac'],
                            ['running', 'running'])
    self.checkString(1, len(out[0]))
    self.checkString(1, len(out[1]))

  def testRunHooksDepsOs(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac', '--revision', 'src@1'])
    out = self.parseGclient(['runhooks', '--deps', 'mac'], [])
    self.assertEquals([], out)

  def testRevInfo(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo', '--deps', 'mac'])
    out = ('src: %(base)s/src\n'
           'src/file/other: File("%(base)s/other/DEPS")\n'
           'src/other: %(base)s/other\n'
           'src/third_party/foo: %(base)s/third_party/foo@1\n' %
          { 'base': self.svn_base + 'trunk' })
    self.check((out, '', 0), results)
    results = self.gclient(['revinfo', '--deps', 'mac', '--actual'])
    out = ('src: %(base)s/src@2\n'
           'src/file/other: %(base)s/other/DEPS@2\n'
           'src/other: %(base)s/other@2\n'
           'src/third_party/foo: %(base)s/third_party/foo@1\n' %
          { 'base': self.svn_base + 'trunk' })
    self.check((out, '', 0), results)
    results = self.gclient(['revinfo', '--deps', 'mac', '--snapshot'])
    out = ('# Snapshot generated with gclient revinfo --snapshot\n'
           'solutions = [\n'
           '  { "name"        : "src",\n'
           '    "url"         : "%(base)s/src",\n'
           '    "deps_file"   : "DEPS",\n'
           '    "managed"     : True,\n'
           '    "custom_deps" : {\n'
           '      "foo/bar": None,\n'
           '      "invalid": None,\n'
           '      "src/file/other": \'%(base)s/other/DEPS@2\',\n'
           '      "src/other": \'%(base)s/other@2\',\n'
           '      "src/third_party/foo": '
               '\'%(base)s/third_party/foo@1\',\n'
           '    },\n'
           '    "safesync_url": "",\n'
           '  },\n'
           ']\n\n' %
          { 'base': self.svn_base + 'trunk' })
    self.check((out, '', 0), results)

  def testRevInfoAltDeps(self):
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/', '--deps-file',
                  'DEPS.alt'])
    self.gclient(['sync'])
    results = self.gclient(['revinfo', '--snapshot'])
    out = ('# Snapshot generated with gclient revinfo --snapshot\n'
           'solutions = [\n'
           '  { "name"        : "src",\n'
           '    "url"         : "%(base)s/src",\n'
           '    "deps_file"   : "DEPS.alt",\n'
           '    "managed"     : True,\n'
           '    "custom_deps" : {\n'
           '      "src/other2": \'%(base)s/other@2\',\n'
           '    },\n'
           '    "safesync_url": "",\n'
           '  },\n'
           ']\n\n' %
          { 'base': self.svn_base + 'trunk' })
    self.check((out, '', 0), results)


  def testWrongDirectory(self):
    # Check that we're not using a .gclient configuration which only talks
    # about a subdirectory src when we're in a different subdirectory src-other.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync'])
    other_src = join(self.root_dir, 'src-other')
    os.mkdir(other_src)
    res = ('', 'Error: client not configured; see \'gclient config\'\n', 1)
    self.check(res, self.gclient(['status'], other_src))

  def testCorrectDirectory(self):
    # Check that when we're in the subdirectory src, the .gclient configuration
    # is used.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync'])
    src = join(self.root_dir, 'src')
    res = self.gclient(['status', '--jobs', '1'], src)
    self.checkBlock(res[0], [('running', src)])

  def testInitialCheckoutNotYetDone(self):
    # Check that gclient can be executed when the initial checkout hasn't been
    # done yet.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.parseGclient(
        ['sync', '--jobs', '1'],
        ['running', 'running',
         # This is due to the way svn update is called for a
         # single file when File() is used in a DEPS file.
         ('running', os.path.join(self.root_dir, 'src', 'file', 'other')),
         'running', 'running', 'running', 'running'])

  def testInitialCheckoutFailed(self):
    # Check that gclient can be executed from an arbitrary sub directory if the
    # initial checkout has failed.
    if not self.enabled:
      return
    self.gclient(['config', self.svn_base + 'trunk/src/'])
    self.gclient(['sync'])
    # Cripple the checkout.
    os.remove(join(self.root_dir, '.gclient_entries'))
    src = join(self.root_dir, 'src')
    res = self.gclient(['sync', '--jobs', '1'], src)
    self.checkBlock(res[0],
                    ['running', 'running', 'running'])


class GClientSmokeGIT(GClientSmokeBase):
  def setUp(self):
    super(GClientSmokeGIT, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_git()

  def testSync(self):
    if not self.enabled:
      return
    # TODO(maruel): safesync.
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    # Test unversioned checkout.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
                      ['running', 'running', 'running', 'running', 'running'])
    # TODO(maruel): http://crosbug.com/3582 hooks run even if not matching, must
    # add sync parsing to get the list of updated files.
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

    # Manually remove git_hooked1 before synching to make sure it's not
    # recreated.
    os.remove(join(self.root_dir, 'src', 'git_hooked1'))

    # Test incremental versioned sync: sync backward.
    self.parseGclient(['sync', '--jobs', '1', '--revision',
                       'src@' + self.githash('repo_1', 1),
                       '--deps', 'mac', '--delete_unversioned_trees'],
                       ['running', 'running', 'deleting'])
    tree = self.mangle_git_tree(('repo_1@1', 'src'),
                                ('repo_2@2', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_4@2', 'src/repo4'))
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running', 'running'])
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_3@2', 'src/repo2/repo_renamed'),
                                ('repo_4@2', 'src/repo4'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

  def testSyncIgnoredSolutionName(self):
    """TODO(maruel): This will become an error soon."""
    if not self.enabled:
      return
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.parseGclient(
        ['sync', '--deps', 'mac', '--jobs', '1',
         '--revision', 'invalid@' + self.githash('repo_1', 1)],
        ['running', 'running', 'running', 'running', 'running'],
        'Please fix your script, having invalid --revision flags '
            'will soon considered an error.\n')
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

  def testSyncNoSolutionName(self):
    if not self.enabled:
      return
    # When no solution name is provided, gclient uses the first solution listed.
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1',
                       '--revision', self.githash('repo_1', 1)],
                      ['running', 'running', 'running', 'running'])
    tree = self.mangle_git_tree(('repo_1@1', 'src'),
                                ('repo_2@2', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_4@2', 'src/repo4'))
    self.assertTree(tree)

  def testSyncJobs(self):
    if not self.enabled:
      return
    # TODO(maruel): safesync.
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    # Test unversioned checkout.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '8'],
                      ['running', 'running', 'running', 'running', 'running'],
                      untangle=True)
    # TODO(maruel): http://crosbug.com/3582 hooks run even if not matching, must
    # add sync parsing to get the list of updated files.
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

    # Manually remove git_hooked1 before synching to make sure it's not
    # recreated.
    os.remove(join(self.root_dir, 'src', 'git_hooked1'))

    # Test incremental versioned sync: sync backward.
    self.parseGclient(
        ['sync', '--revision', 'src@' + self.githash('repo_1', 1),
          '--deps', 'mac', '--delete_unversioned_trees', '--jobs', '8'],
        ['running', 'running', 'deleting'],
        untangle=True)
    tree = self.mangle_git_tree(('repo_1@1', 'src'),
                                ('repo_2@2', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_4@2', 'src/repo4'))
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)
    # Test incremental sync: delete-unversioned_trees isn't there.
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '8'],
        ['running', 'running', 'running'], untangle=True)
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_3@2', 'src/repo2/repo_renamed'),
                                ('repo_4@2', 'src/repo4'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

  def testRevertAndStatus(self):
    """TODO(maruel): Remove this line once this test is fixed."""
    if not self.enabled:
      return
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    # Tested in testSync.
    self.gclient(['sync', '--deps', 'mac'])
    write(join(self.root_dir, 'src', 'repo2', 'hi'), 'Hey!')

    out = self.parseGclient(['status', '--deps', 'mac'], [])
    # TODO(maruel): http://crosbug.com/3584 It should output the unversioned
    # files.
    self.assertEquals(0, len(out))

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert', '--deps', 'mac'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3583 It just runs the hooks right now.
    self.assertEquals(13, len(out))
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    # TODO(maruel): http://crosbug.com/3583 This file should have been removed.
    tree[join('src', 'repo2', 'hi')] = 'Hey!'
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

    results = self.gclient(['status', '--deps', 'mac'])
    out = results[0].splitlines(False)
    # TODO(maruel): http://crosbug.com/3584 It should output the unversioned
    # files.
    self.assertEquals(0, len(out))

  def testRunHooks(self):
    if not self.enabled:
      return
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.gclient(['sync', '--deps', 'mac'])
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

    os.remove(join(self.root_dir, 'src', 'git_hooked1'))
    os.remove(join(self.root_dir, 'src', 'git_hooked2'))
    # runhooks runs all hooks even if not matching by design.
    out = self.parseGclient(['runhooks', '--deps', 'mac'],
                            ['running', 'running'])
    self.assertEquals(1, len(out[0]))
    self.assertEquals(1, len(out[1]))
    tree = self.mangle_git_tree(('repo_1@2', 'src'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    self.assertTree(tree)

  def testRevInfo(self):
    if not self.enabled:
      return
    self.gclient(['config', self.git_base + 'repo_1', '--name', 'src'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo', '--deps', 'mac'])
    out = ('src: %(base)srepo_1\n'
           'src/repo2: %(base)srepo_2@%(hash2)s\n'
           'src/repo2/repo_renamed: %(base)srepo_3\n' %
          {
            'base': self.git_base,
            'hash2': self.githash('repo_2', 1)[:7],
          })
    self.check((out, '', 0), results)
    results = self.gclient(['revinfo', '--deps', 'mac', '--actual'])
    out = ('src: %(base)srepo_1@%(hash1)s\n'
           'src/repo2: %(base)srepo_2@%(hash2)s\n'
           'src/repo2/repo_renamed: %(base)srepo_3@%(hash3)s\n' %
          {
            'base': self.git_base,
            'hash1': self.githash('repo_1', 2),
            'hash2': self.githash('repo_2', 1),
            'hash3': self.githash('repo_3', 2),
          })
    self.check((out, '', 0), results)


class GClientSmokeBoth(GClientSmokeBase):
  def setUp(self):
    super(GClientSmokeBoth, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_svn() and self.FAKE_REPOS.set_up_git()

  def testMultiSolutions(self):
    if not self.enabled:
      return
    self.gclient(['config', '--spec',
        'solutions=['
        '{"name": "src",'
        ' "url": "' + self.svn_base + 'trunk/src/"},'
        '{"name": "src-git",'
        '"url": "' + self.git_base + 'repo_1"}]'])
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running', 'running',
         # This is due to the way svn update is called for a single
         # file when File() is used in a DEPS file.
         ('running', self.root_dir + '/src/file/other'),
         'running', 'running', 'running', 'running', 'running', 'running',
         'running', 'running'])
    tree = self.mangle_git_tree(('repo_1@2', 'src-git'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree.update(self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other')))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testMultiSolutionsJobs(self):
    print >> sys.stderr, (
        'Warning: testMultiSolutionsJobs is temporarily disabled')
    return
    # unreachable code
    # pylint: disable=W0101
    if not self.enabled:
      return
    self.gclient(['config', '--spec',
        'solutions=['
        '{"name": "src",'
        ' "url": "' + self.svn_base + 'trunk/src/"},'
        '{"name": "src-git",'
        '"url": "' + self.git_base + 'repo_1"}]'])
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '8'],
        ['running', 'running', 'running',
         # This is due to the way svn update is called for a single
         # file when File() is used in a DEPS file.
         ('running', self.root_dir + '/src/file/other'),
         'running', 'running', 'running', 'running', 'running', 'running',
         'running', 'running'],
        untangle=True)
    tree = self.mangle_git_tree(('repo_1@2', 'src-git'),
                                ('repo_2@1', 'src/repo2'),
                                ('repo_3@2', 'src/repo2/repo_renamed'))
    tree.update(self.mangle_svn_tree(
        ('trunk/src@2', 'src'),
        ('trunk/third_party/foo@1', 'src/third_party/foo'),
        ('trunk/other@2', 'src/other')))
    tree['src/file/other/DEPS'] = (
        self.FAKE_REPOS.svn_revs[2]['trunk/other/DEPS'])
    tree['src/git_hooked1'] = 'git_hooked1'
    tree['src/git_hooked2'] = 'git_hooked2'
    tree['src/svn_hooked1'] = 'svn_hooked1'
    self.assertTree(tree)

  def testMultiSolutionsMultiRev(self):
    if not self.enabled:
      return
    self.gclient(['config', '--spec',
        'solutions=['
        '{"name": "src",'
        ' "url": "' + self.svn_base + 'trunk/src/"},'
        '{"name": "src-git",'
        '"url": "' + self.git_base + 'repo_1"}]'])
    self.parseGclient(
        ['sync', '--deps', 'mac', '--jobs', '1', '--revision', '1',
         '-r', 'src-git@' + self.githash('repo_1', 1)],
        ['running', 'running', 'running', 'running',
         'running', 'running', 'running', 'running'])
    tree = self.mangle_git_tree(('repo_1@1', 'src-git'),
                                ('repo_2@2', 'src/repo2'),
                                ('repo_3@1', 'src/repo2/repo3'),
                                ('repo_4@2', 'src/repo4'))
    tree.update(self.mangle_svn_tree(
        ('trunk/src@1', 'src'),
        ('trunk/third_party/foo@2', 'src/third_party/fpp'),
        ('trunk/other@1', 'src/other'),
        ('trunk/third_party/foo@2', 'src/third_party/prout')))
    self.assertTree(tree)

  def testRevInfo(self):
    if not self.enabled:
      return
    self.gclient(['config', '--spec',
        'solutions=['
        '{"name": "src",'
        ' "url": "' + self.svn_base + 'trunk/src/"},'
        '{"name": "src-git",'
        '"url": "' + self.git_base + 'repo_1"}]'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo', '--deps', 'mac'])
    out = ('src: %(svn_base)s/src/\n'
           'src-git: %(git_base)srepo_1\n'
           'src/file/other: File("%(svn_base)s/other/DEPS")\n'
           'src/other: %(svn_base)s/other\n'
           'src/repo2: %(git_base)srepo_2@%(hash2)s\n'
           'src/repo2/repo_renamed: %(git_base)srepo_3\n'
           'src/third_party/foo: %(svn_base)s/third_party/foo@1\n') % {
               'svn_base': self.svn_base + 'trunk',
               'git_base': self.git_base,
               'hash2': self.githash('repo_2', 1)[:7],
          }
    self.check((out, '', 0), results)
    results = self.gclient(['revinfo', '--deps', 'mac', '--actual'])
    out = ('src: %(svn_base)s/src/@2\n'
           'src-git: %(git_base)srepo_1@%(hash1)s\n'
           'src/file/other: %(svn_base)s/other/DEPS@2\n'
           'src/other: %(svn_base)s/other@2\n'
           'src/repo2: %(git_base)srepo_2@%(hash2)s\n'
           'src/repo2/repo_renamed: %(git_base)srepo_3@%(hash3)s\n'
           'src/third_party/foo: %(svn_base)s/third_party/foo@1\n') % {
               'svn_base': self.svn_base + 'trunk',
               'git_base': self.git_base,
               'hash1': self.githash('repo_1', 2),
               'hash2': self.githash('repo_2', 1),
               'hash3': self.githash('repo_3', 2),
          }
    self.check((out, '', 0), results)

  def testRecurse(self):
    if not self.enabled:
      return
    self.gclient(['config', '--spec',
        'solutions=['
        '{"name": "src",'
        ' "url": "' + self.svn_base + 'trunk/src/"},'
        '{"name": "src-git",'
        '"url": "' + self.git_base + 'repo_1"}]'])
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['recurse', 'sh', '-c',
                            'echo $GCLIENT_SCM,$GCLIENT_URL,`pwd`'])

    entries = [tuple(line.split(','))
               for line in results[0].strip().split('\n')]
    logging.debug(entries)

    bases = {'svn': self.svn_base, 'git': self.git_base}
    expected_source = [
        ('svn', 'trunk/src/', 'src'),
        ('git', 'repo_1', 'src-git'),
        ('svn', 'trunk/other', 'src/other'),
        ('git', 'repo_2@' + self.githash('repo_2', 1)[:7], 'src/repo2'),
        ('git', 'repo_3', 'src/repo2/repo_renamed'),
        ('svn', 'trunk/third_party/foo@1', 'src/third_party/foo'),
      ]
    expected = [(scm, bases[scm] + url, os.path.join(self.root_dir, path))
                for (scm, url, path) in expected_source]

    self.assertEquals(sorted(entries), sorted(expected))


class GClientSmokeFromCheckout(GClientSmokeBase):
  # WebKit abuses this. It has a .gclient and a DEPS from a checkout.
  def setUp(self):
    super(GClientSmokeFromCheckout, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_svn()
    os.rmdir(self.root_dir)
    if self.enabled:
      usr, pwd = self.FAKE_REPOS.USERS[0]
      subprocess2.check_call(
          ['svn', 'checkout', self.svn_base + '/trunk/webkit',
           self.root_dir, '-q',
           '--non-interactive', '--no-auth-cache',
           '--username', usr, '--password', pwd])

  def testSync(self):
    if not self.enabled:
      return
    self.parseGclient(['sync', '--deps', 'mac', '--jobs', '1'],
        ['running', 'running'])
    tree = self.mangle_svn_tree(
        ('trunk/webkit@2', ''),
        ('trunk/third_party/foo@1', 'foo/bar'))
    self.assertTree(tree)

  def testRevertAndStatus(self):
    if not self.enabled:
      return
    self.gclient(['sync'])

    # TODO(maruel): This is incorrect.
    out = self.parseGclient(['status', '--deps', 'mac', '--jobs', '1'], [])

    # Revert implies --force implies running hooks without looking at pattern
    # matching.
    results = self.gclient(['revert', '--deps', 'mac', '--jobs', '1'])
    out = self.splitBlock(results[0])
    self.assertEquals(2, len(out))
    self.checkString(2, len(out[0]))
    self.checkString(2, len(out[1]))
    self.checkString('foo', out[1][1])
    self.checkString('', results[1])
    self.assertEquals(0, results[2])
    tree = self.mangle_svn_tree(
        ('trunk/webkit@2', ''),
        ('trunk/third_party/foo@1', 'foo/bar'))
    self.assertTree(tree)

    # TODO(maruel): This is incorrect.
    out = self.parseGclient(['status', '--deps', 'mac'], [])

  def testRunHooks(self):
    if not self.enabled:
      return
    # Hooks aren't really tested for now since there is no hook defined.
    self.gclient(['sync', '--deps', 'mac'])
    out = self.parseGclient(['runhooks', '--deps', 'mac'], ['running'])
    self.assertEquals(1, len(out))
    self.assertEquals(2, len(out[0]))
    self.assertEquals(3, len(out[0][0]))
    self.checkString('foo', out[0][1])
    tree = self.mangle_svn_tree(
        ('trunk/webkit@2', ''),
        ('trunk/third_party/foo@1', 'foo/bar'))
    self.assertTree(tree)

  def testRevInfo(self):
    if not self.enabled:
      return
    self.gclient(['sync', '--deps', 'mac'])
    results = self.gclient(['revinfo', '--deps', 'mac'])
    expected = (
        './: None\nfoo/bar: %strunk/third_party/foo@1\n' % self.svn_base,
        '', 0)
    self.check(expected, results)
    # TODO(maruel): To be added after the refactor.
    #results = self.gclient(['revinfo', '--snapshot'])
    #expected = (
    #    './: None\nfoo/bar: %strunk/third_party/foo@1\n' % self.svn_base,
    #    '', 0)
    #self.check(expected, results)

  def testRest(self):
    if not self.enabled:
      return
    self.gclient(['sync'])
    # TODO(maruel): This is incorrect, it should run on ./ too.
    self.parseGclient(
        ['cleanup', '--deps', 'mac', '--verbose', '--jobs', '1'],
        [('running', join(self.root_dir, 'foo', 'bar'))])
    self.parseGclient(
        ['diff', '--deps', 'mac', '--verbose', '--jobs', '1'],
        [('running', join(self.root_dir, 'foo', 'bar'))])


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)

  if '-c' in sys.argv:
    COVERAGE = True
    sys.argv.remove('-c')
    if os.path.exists('.coverage'):
      os.remove('.coverage')
    os.environ['COVERAGE_FILE'] = os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
        '.coverage')
  unittest.main()
