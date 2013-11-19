#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_common.py"""

import binascii
import collections
import os
import signal
import sys
import tempfile
import time
import unittest

DEPOT_TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, DEPOT_TOOLS_ROOT)

from testing_support import coverage_utils
from testing_support import git_test_utils


class GitCommonTestBase(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    super(GitCommonTestBase, cls).setUpClass()
    import git_common
    cls.gc = git_common


class Support(GitCommonTestBase):
  def _testMemoizeOneBody(self, threadsafe):
    calls = collections.defaultdict(int)
    def double_if_even(val):
      calls[val] += 1
      return val * 2 if val % 2 == 0 else None
    # Use this explicitly as a wrapper fn instead of a decorator. Otherwise
    # pylint crashes (!!)
    double_if_even = self.gc.memoize_one(threadsafe=threadsafe)(double_if_even)

    self.assertEqual(4, double_if_even(2))
    self.assertEqual(4, double_if_even(2))
    self.assertEqual(None, double_if_even(1))
    self.assertEqual(None, double_if_even(1))
    self.assertDictEqual({1: 2, 2: 1}, calls)

    double_if_even.set(10, 20)
    self.assertEqual(20, double_if_even(10))
    self.assertDictEqual({1: 2, 2: 1}, calls)

    double_if_even.clear()
    self.assertEqual(4, double_if_even(2))
    self.assertEqual(4, double_if_even(2))
    self.assertEqual(None, double_if_even(1))
    self.assertEqual(None, double_if_even(1))
    self.assertEqual(20, double_if_even(10))
    self.assertDictEqual({1: 4, 2: 2, 10: 1}, calls)

  def testMemoizeOne(self):
    self._testMemoizeOneBody(threadsafe=False)

  def testMemoizeOneThreadsafe(self):
    self._testMemoizeOneBody(threadsafe=True)


def slow_square(i):
  """Helper for ScopedPoolTest.

  Must be global because non top-level functions aren't pickleable.
  """
  return i ** 2


class ScopedPoolTest(GitCommonTestBase):
  CTRL_C = signal.CTRL_C_EVENT if sys.platform == 'win32' else signal.SIGINT

  def testThreads(self):
    result = []
    with self.gc.ScopedPool(kind='threads') as pool:
      result = list(pool.imap(slow_square, xrange(10)))
    self.assertEqual([0, 1, 4, 9, 16, 25, 36, 49, 64, 81], result)

  def testThreadsCtrlC(self):
    result = []
    with self.assertRaises(KeyboardInterrupt):
      with self.gc.ScopedPool(kind='threads') as pool:
        # Make sure this pool is interrupted in mid-swing
        for i in pool.imap(slow_square, xrange(1000000)):
          if i > 32:
            os.kill(os.getpid(), self.CTRL_C)
          result.append(i)
    self.assertEqual([0, 1, 4, 9, 16, 25], result)

  def testProcs(self):
    result = []
    with self.gc.ScopedPool() as pool:
      result = list(pool.imap(slow_square, xrange(10)))
    self.assertEqual([0, 1, 4, 9, 16, 25, 36, 49, 64, 81], result)

  def testProcsCtrlC(self):
    result = []
    with self.assertRaises(KeyboardInterrupt):
      with self.gc.ScopedPool() as pool:
        # Make sure this pool is interrupted in mid-swing
        for i in pool.imap(slow_square, xrange(1000000)):
          if i > 32:
            os.kill(os.getpid(), self.CTRL_C)
          result.append(i)
    self.assertEqual([0, 1, 4, 9, 16, 25], result)


class ProgressPrinterTest(GitCommonTestBase):
  class FakeStream(object):
    def __init__(self):
      self.data = set()
      self.count = 0

    def write(self, line):
      self.data.add(line)

    def flush(self):
      self.count += 1

  @unittest.expectedFailure
  def testBasic(self):
    """This test is probably racy, but I don't have a better alternative."""
    fmt = '%(count)d/10'
    stream = self.FakeStream()

    pp = self.gc.ProgressPrinter(fmt, enabled=True, stream=stream, period=0.01)
    with pp as inc:
      for _ in xrange(10):
        time.sleep(0.02)
        inc()

    filtered = set(x.strip() for x in stream.data)
    rslt = set(fmt % {'count': i} for i in xrange(11))
    self.assertSetEqual(filtered, rslt)
    self.assertGreaterEqual(stream.count, 10)


class GitReadOnlyFunctionsTest(git_test_utils.GitRepoReadOnlyTestBase,
                               GitCommonTestBase):
  REPO = """
  A B C D
    B E D
  """

  COMMIT_A = {
    'some/files/file1': {'data': 'file1'},
    'some/files/file2': {'data': 'file2'},
    'some/files/file3': {'data': 'file3'},
    'some/other/file':  {'data': 'otherfile'},
  }

  COMMIT_C = {
    'some/files/file2': {
      'mode': 0755,
      'data': 'file2 - vanilla'},
  }

  COMMIT_E = {
    'some/files/file2': {'data': 'file2 - merged'},
  }

  COMMIT_D = {
    'some/files/file2': {'data': 'file2 - vanilla\nfile2 - merged'},
  }

  def testHashes(self):
    ret = self.repo.run(
      self.gc.hashes, *[
        'master',
        'master~3',
        self.repo['E']+'~',
        self.repo['D']+'^2',
        'tag_C^{}',
      ]
    )
    self.assertEqual([
      self.repo['D'],
      self.repo['A'],
      self.repo['B'],
      self.repo['E'],
      self.repo['C'],
    ], ret)

  def testParseCommitrefs(self):
    ret = self.repo.run(
      self.gc.parse_commitrefs, *[
        'master',
        'master~3',
        self.repo['E']+'~',
        self.repo['D']+'^2',
        'tag_C^{}',
      ]
    )
    self.assertEqual(ret, map(binascii.unhexlify, [
      self.repo['D'],
      self.repo['A'],
      self.repo['B'],
      self.repo['E'],
      self.repo['C'],
    ]))

    with self.assertRaisesRegexp(Exception, r"one of \('master', 'bananas'\)"):
      self.repo.run(self.gc.parse_commitrefs, 'master', 'bananas')

  def testTree(self):
    tree = self.repo.run(self.gc.tree, 'master:some/files')
    file1 = self.COMMIT_A['some/files/file1']['data']
    file2 = self.COMMIT_D['some/files/file2']['data']
    file3 = self.COMMIT_A['some/files/file3']['data']
    self.assertEquals(
        tree['file1'],
        ('100644', 'blob', git_test_utils.git_hash_data(file1)))
    self.assertEquals(
        tree['file2'],
        ('100755', 'blob', git_test_utils.git_hash_data(file2)))
    self.assertEquals(
        tree['file3'],
        ('100644', 'blob', git_test_utils.git_hash_data(file3)))

    tree = self.repo.run(self.gc.tree, 'master:some')
    self.assertEquals(len(tree), 2)
    # Don't check the tree hash because we're lazy :)
    self.assertEquals(tree['files'][:2], ('040000', 'tree'))

    tree = self.repo.run(self.gc.tree, 'master:wat')
    self.assertEqual(tree, None)

  def testTreeRecursive(self):
    tree = self.repo.run(self.gc.tree, 'master:some', recurse=True)
    file1 = self.COMMIT_A['some/files/file1']['data']
    file2 = self.COMMIT_D['some/files/file2']['data']
    file3 = self.COMMIT_A['some/files/file3']['data']
    other = self.COMMIT_A['some/other/file']['data']
    self.assertEquals(
        tree['files/file1'],
        ('100644', 'blob', git_test_utils.git_hash_data(file1)))
    self.assertEquals(
        tree['files/file2'],
        ('100755', 'blob', git_test_utils.git_hash_data(file2)))
    self.assertEquals(
        tree['files/file3'],
        ('100644', 'blob', git_test_utils.git_hash_data(file3)))
    self.assertEquals(
        tree['other/file'],
        ('100644', 'blob', git_test_utils.git_hash_data(other)))


class GitMutableFunctionsTest(git_test_utils.GitRepoReadWriteTestBase,
                              GitCommonTestBase):
  REPO = ''

  def _intern_data(self, data):
    with tempfile.TemporaryFile() as f:
      f.write(data)
      f.seek(0)
      return self.repo.run(self.gc.intern_f, f)

  def testInternF(self):
    data = 'CoolBobcatsBro'
    data_hash = self._intern_data(data)
    self.assertEquals(git_test_utils.git_hash_data(data), data_hash)
    self.assertEquals(data, self.repo.git('cat-file', 'blob', data_hash).stdout)

  def testMkTree(self):
    tree = {}
    for i in 1, 2, 3:
      name = 'file%d' % i
      tree[name] = ('100644', 'blob', self._intern_data(name))
    tree_hash = self.repo.run(self.gc.mktree, tree)
    self.assertEquals('37b61866d6e061c4ba478e7eb525be7b5752737d', tree_hash)


if __name__ == '__main__':
  sys.exit(coverage_utils.covered_main(
    os.path.join(DEPOT_TOOLS_ROOT, 'git_common.py')
  ))
