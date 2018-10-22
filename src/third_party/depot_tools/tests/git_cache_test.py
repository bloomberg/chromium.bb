#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_cache.py"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

DEPOT_TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, DEPOT_TOOLS_ROOT)

from testing_support import coverage_utils
import git_cache

class GitCacheTest(unittest.TestCase):
  def setUp(self):
    self.cache_dir = tempfile.mkdtemp(prefix='git_cache_test_')
    self.addCleanup(shutil.rmtree, self.cache_dir, ignore_errors=True)
    self.origin_dir = tempfile.mkdtemp(suffix='origin.git')
    self.addCleanup(shutil.rmtree, self.origin_dir, ignore_errors=True)
    git_cache.Mirror.SetCachePath(self.cache_dir)

  def git(self, cmd, cwd=None):
    cwd = cwd or self.origin_dir
    subprocess.check_call(['git'] + cmd, cwd=cwd)

  def testParseFetchSpec(self):
    testData = [
        ([], []),
        (['master'], [('+refs/heads/master:refs/heads/master',
                       r'\+refs/heads/master:.*')]),
        (['master/'], [('+refs/heads/master:refs/heads/master',
                       r'\+refs/heads/master:.*')]),
        (['+master'], [('+refs/heads/master:refs/heads/master',
                       r'\+refs/heads/master:.*')]),
        (['refs/heads/*'], [('+refs/heads/*:refs/heads/*',
                            r'\+refs/heads/\*:.*')]),
        (['foo/bar/*', 'baz'], [('+refs/heads/foo/bar/*:refs/heads/foo/bar/*',
                                r'\+refs/heads/foo/bar/\*:.*'),
                               ('+refs/heads/baz:refs/heads/baz',
                                r'\+refs/heads/baz:.*')]),
        (['refs/foo/*:refs/bar/*'], [('+refs/foo/*:refs/bar/*',
                                      r'\+refs/foo/\*:.*')])
        ]

    mirror = git_cache.Mirror('test://phony.example.biz')
    for fetch_specs, expected in testData:
      mirror = git_cache.Mirror('test://phony.example.biz', refs=fetch_specs)
      self.assertItemsEqual(mirror.fetch_specs, expected)

  def testPopulate(self):
    self.git(['init', '-q'])
    with open(os.path.join(self.origin_dir, 'foo'), 'w') as f:
      f.write('touched\n')
    self.git(['add', 'foo'])
    self.git(['commit', '-m', 'foo'])

    mirror = git_cache.Mirror(self.origin_dir)
    mirror.populate()

  def testPopulateResetFetchConfig(self):
    self.git(['init', '-q'])
    with open(os.path.join(self.origin_dir, 'foo'), 'w') as f:
      f.write('touched\n')
    self.git(['add', 'foo'])
    self.git(['commit', '-m', 'foo'])

    mirror = git_cache.Mirror(self.origin_dir)
    mirror.populate()

    # Add a bad refspec to the cache's fetch config.
    cache_dir = os.path.join(
        self.cache_dir, mirror.UrlToCacheDir(self.origin_dir))
    self.git(['config', '--add', 'remote.origin.fetch',
              '+refs/heads/foo:refs/heads/foo'],
             cwd=cache_dir)

    mirror.populate(reset_fetch_config=True)


  def testPopulateResetFetchConfigEmptyFetchConfig(self):
    self.git(['init', '-q'])
    with open(os.path.join(self.origin_dir, 'foo'), 'w') as f:
      f.write('touched\n')
    self.git(['add', 'foo'])
    self.git(['commit', '-m', 'foo'])

    mirror = git_cache.Mirror(self.origin_dir)
    mirror.populate(reset_fetch_config=True)


class GitCacheDirTest(unittest.TestCase):
  def setUp(self):
    try:
      delattr(git_cache.Mirror, 'cachepath')
    except AttributeError:
      pass
    super(GitCacheDirTest, self).setUp()

  def tearDown(self):
    try:
      delattr(git_cache.Mirror, 'cachepath')
    except AttributeError:
      pass
    super(GitCacheDirTest, self).tearDown()

  def test_git_config_read(self):
    (fd, tmpFile) = tempfile.mkstemp()
    old = git_cache.Mirror._GIT_CONFIG_LOCATION
    try:
      try:
        os.write(fd, '[cache]\n  cachepath="hello world"\n')
      finally:
        os.close(fd)

      git_cache.Mirror._GIT_CONFIG_LOCATION = ['-f', tmpFile]

      self.assertEqual(git_cache.Mirror.GetCachePath(), 'hello world')
    finally:
      git_cache.Mirror._GIT_CONFIG_LOCATION = old
      os.remove(tmpFile)

  def test_environ_read(self):
    path = os.environ.get('GIT_CACHE_PATH')
    config = os.environ.get('GIT_CONFIG')
    try:
      os.environ['GIT_CACHE_PATH'] = 'hello world'
      os.environ['GIT_CONFIG'] = 'disabled'

      self.assertEqual(git_cache.Mirror.GetCachePath(), 'hello world')
    finally:
      for name, val in zip(('GIT_CACHE_PATH', 'GIT_CONFIG'), (path, config)):
        if val is None:
          os.environ.pop(name, None)
        else:
          os.environ[name] = val

  def test_manual_set(self):
    git_cache.Mirror.SetCachePath('hello world')
    self.assertEqual(git_cache.Mirror.GetCachePath(), 'hello world')

  def test_unconfigured(self):
    path = os.environ.get('GIT_CACHE_PATH')
    config = os.environ.get('GIT_CONFIG')
    try:
      os.environ.pop('GIT_CACHE_PATH', None)
      os.environ['GIT_CONFIG'] = 'disabled'

      with self.assertRaisesRegexp(RuntimeError, 'cache\.cachepath'):
        git_cache.Mirror.GetCachePath()

      # negatively cached value still raises
      with self.assertRaisesRegexp(RuntimeError, 'cache\.cachepath'):
        git_cache.Mirror.GetCachePath()
    finally:
      for name, val in zip(('GIT_CACHE_PATH', 'GIT_CONFIG'), (path, config)):
        if val is None:
          os.environ.pop(name, None)
        else:
          os.environ[name] = val


if __name__ == '__main__':
  sys.exit(coverage_utils.covered_main((
    os.path.join(DEPOT_TOOLS_ROOT, 'git_cache.py')
  ), required_percentage=0))
