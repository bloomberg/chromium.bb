#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile
import traceback
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


# This function is intended to be used as a decorator.  It is for
# noisy tests.  This function wraps a test method to redirect
# stdout/stderr to a temporary file, and reveals the output only if
# the test fails.
def Quieten(func):
  def Wrapper(*args):
    temp_fd, filename = tempfile.mkstemp()
    os.unlink(filename)
    temp_fh = os.fdopen(temp_fd, "w+")
    saved_stdout = os.dup(1)
    saved_stderr = os.dup(2)
    os.dup2(temp_fh.fileno(), 1)
    os.dup2(temp_fh.fileno(), 2)
    try:
      try:
        func(*args)
      finally:
        os.dup2(saved_stdout, 1)
        os.dup2(saved_stderr, 2)
        os.close(saved_stdout)
        os.close(saved_stderr)
    except Exception:
      temp_fh.seek(0)
      output = temp_fh.read()
      error = "".join(traceback.format_exception(*sys.exc_info())).rstrip("\n")
      raise Exception("Test failed with the following output:\n\n%s\n%s" %
                      (output, error))
  return Wrapper


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

  def GetId(self):
    return ["Dir", [entry.GetId() for leafname, entry in self._entries]]


class File(dirtree.DirTree):

  def __init__(self, data):
    self._data = data

  def WriteTree(self, dest_path):
    dirtree.WriteFile(dest_path, self._data)

  def GetId(self):
    return ["File", self._data]


example_tree = Dir([("foo-1.0", Dir([("README", File("hello"))]))])

tree1 = Dir([("foo", File("a\nhello world\nb"))])
tree2 = Dir([("foo", File("a\nbye\nb"))])


class DirTreeTests(TempDirTestCase):

  def _RealizeTree(self, tree):
    temp_dir = self.MakeTempDir()
    tree.WriteTree(temp_dir)
    return temp_dir

  def _CheckGetId(self, tree):
    # Just check that GetId() is callable and returns a stable value.
    self.assertEquals(tree.GetId(), tree.GetId())

  def test_untar(self):
    temp_dir = self._RealizeTree(example_tree)
    tar_file = os.path.join(temp_dir, "foo-1.0.tar.gz")
    subprocess.check_call(["tar", "-cf", tar_file, "foo-1.0"],
                          cwd=temp_dir)

    tree = dirtree.TarballTree(tar_file)
    dest_dir = self._RealizeTree(tree)
    self.assertEquals(os.listdir(dest_dir), ["README"])
    self._CheckGetId(tree)

    tree = dirtree.MultiTarballTree([tar_file, tar_file])
    dest_dir = self._RealizeTree(tree)
    self.assertEquals(os.listdir(dest_dir), ["README"])
    self._CheckGetId(tree)

  @Quieten
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
    tree = dirtree.PatchedTree(tree1, [diff_file])
    result_dir = self._RealizeTree(tree)
    rc = subprocess.call(["diff", "-urN",
                          os.path.join(temp_dir, "b"), result_dir])
    self.assertEquals(rc, 0)
    self._CheckGetId(tree)

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


if __name__ == "__main__":
  unittest.main()
