#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import shutil
import subprocess
import tempfile
import unittest

import dirtree


# Public domain, from
# http://lackingrhoticity.blogspot.com/2008/11/tempdirtestcase-python-unittest-helper.html
class TempDirTestCase(unittest.TestCase):

  def setUp(self):
    self._on_teardown = []

  def MakeTempDir(self):
    temp_dir = tempfile.mkdtemp(prefix="tmp-%s-" % self.__class__.__name__)
    def TearDown():
      shutil.rmtree(temp_dir)
    self._on_teardown.append(TearDown)
    return temp_dir

  def tearDown(self):
    for func in reversed(self._on_teardown):
      func()


class Dir(dirtree.DirTree):

  def __init__(self, entries=()):
    self._entries = entries

  def WriteTree(self, dest_path):
    for leafname, entry in self._entries:
      dest = os.path.join(dest_path, leafname)
      # This is cheating slightly.  DirTrees other than File assume
      # that the directory has already been created.
      if not isinstance(entry, File):
        os.mkdir(dest)
      entry.WriteTree(dest)


class File(dirtree.DirTree):

  def __init__(self, data):
    self._data = data

  def WriteTree(self, dest_path):
    dirtree.WriteFile(dest_path, self._data)


example_tree = Dir([("foo-1.0", Dir([("README", File("hello"))]))])

tree1 = Dir([("foo", File("a\nhello world\nb"))])
tree2 = Dir([("foo", File("a\nbye\nb"))])


class DirTreeTests(TempDirTestCase):

  def _RealizeTree(self, tree):
    temp_dir = self.MakeTempDir()
    tree.WriteTree(temp_dir)
    return temp_dir

  def test_untar(self):
    temp_dir = self._RealizeTree(example_tree)
    tar_file = os.path.join(temp_dir, "foo-1.0.tar.gz")
    subprocess.check_call(["tar", "-cf", tar_file, "foo-1.0"],
                          cwd=temp_dir)

    tree = dirtree.TarballTree(tar_file)
    dest_dir = self._RealizeTree(tree)
    self.assertEquals(os.listdir(dest_dir), ["README"])

    tree = dirtree.MultiTarballTree([tar_file, tar_file])
    dest_dir = self._RealizeTree(tree)
    self.assertEquals(os.listdir(dest_dir), ["README"])

  def test_patch_tree(self):
    temp_dir = self.MakeTempDir()
    os.mkdir(os.path.join(temp_dir, "a"))
    os.mkdir(os.path.join(temp_dir, "b"))
    tree1.WriteTree(os.path.join(temp_dir, "a"))
    tree2.WriteTree(os.path.join(temp_dir, "b"))
    diff_file = os.path.join(temp_dir, "diff")
    rc = subprocess.call(["diff", "-urN", "a", "b"],
                         stdout=open(diff_file, "w"), cwd=temp_dir)
    self.assertEquals(rc, 1)
    result_dir = self._RealizeTree(dirtree.PatchedTree(tree1, [diff_file]))
    rc = subprocess.call(["diff", "-urN",
                          os.path.join(temp_dir, "b"), result_dir])
    self.assertEquals(rc, 0)

    # Check that patch doesn't ask questions if we try re-applying a patch.
    self.assertRaises(
        subprocess.CalledProcessError,
        lambda: self._RealizeTree(
            dirtree.PatchedTree(tree2, [diff_file])))

    # Check that patch doesn't ask questions if the patch fails to
    # apply because a file is missing.
    self.assertRaises(
        subprocess.CalledProcessError,
        lambda: self._RealizeTree(
            dirtree.PatchedTree(dirtree.EmptyTree(), [diff_file])))

  def test_git_tree(self):
    repo_dir = self._RealizeTree(Dir([("myfile", File("My file"))]))
    subprocess.check_call(["git", "init", "-q"], cwd=repo_dir)
    subprocess.check_call(["git", "add", "."], cwd=repo_dir)
    subprocess.check_call(["git", "commit", "-q", "-a", "-m", "initial"],
                          cwd=repo_dir)
    tree = dirtree.GitTree(repo_dir)
    clone_dir = self._RealizeTree(tree)
    self.assertEquals(dirtree.ReadFile(os.path.join(clone_dir, "myfile")),
                      "My file")


if __name__ == "__main__":
  unittest.main()
