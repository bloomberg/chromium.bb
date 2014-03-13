#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_freezer.py"""

import os
import sys

DEPOT_TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, DEPOT_TOOLS_ROOT)

from testing_support import coverage_utils
from testing_support import git_test_utils


class GitFreezeThaw(git_test_utils.GitRepoReadWriteTestBase):
  @classmethod
  def setUpClass(cls):
    super(GitFreezeThaw, cls).setUpClass()
    import git_freezer
    cls.gf = git_freezer

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

  def testNothing(self):
    self.assertIsNotNone(self.repo.run(self.gf.thaw))  # 'Nothing to thaw'
    self.assertIsNotNone(self.repo.run(self.gf.freeze))  # 'Nothing to freeze'

  def testAll(self):
    def inner():
      with open('some/files/file2', 'a') as f2:
        print >> f2, 'cool appended line'
      os.mkdir('some/other_files')
      with open('some/other_files/subdir_file', 'w') as f3:
        print >> f3, 'new file!'
      with open('some/files/file5', 'w') as f5:
        print >> f5, 'New file!1!one!'

      STATUS_1 = '\n'.join((
        ' M some/files/file2',
        'A  some/files/file5',
        '?? some/other_files/'
      )) + '\n'

      self.repo.git('add', 'some/files/file5')

      # Freeze group 1
      self.assertEquals(self.repo.git('status', '--porcelain').stdout, STATUS_1)
      self.assertIsNone(self.gf.freeze())
      self.assertEquals(self.repo.git('status', '--porcelain').stdout, '')

      # Freeze group 2
      with open('some/files/file2', 'a') as f2:
        print >> f2, 'new! appended line!'
      self.assertEquals(self.repo.git('status', '--porcelain').stdout,
                        ' M some/files/file2\n')
      self.assertIsNone(self.gf.freeze())
      self.assertEquals(self.repo.git('status', '--porcelain').stdout, '')

      # Thaw it out!
      self.assertIsNone(self.gf.thaw())
      self.assertIsNotNone(self.gf.thaw())  # One thaw should thaw everything

      self.assertEquals(self.repo.git('status', '--porcelain').stdout, STATUS_1)

    self.repo.run(inner)



if __name__ == '__main__':
  sys.exit(coverage_utils.covered_main(
    os.path.join(DEPOT_TOOLS_ROOT, 'git_freezer.py')
  ))
