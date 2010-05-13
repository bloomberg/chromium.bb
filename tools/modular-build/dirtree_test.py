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
import cmd_env


def ReadFile(filename):
  fh = open(filename, "r")
  try:
    return fh.read()
  finally:
    fh.close()


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


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


class Test(TempDirTestCase):

  def test_untar(self):
    temp_dir = self.MakeTempDir()
    os.mkdir(os.path.join(temp_dir, "foo-1.0"))
    WriteFile(os.path.join(temp_dir, "foo-1.0", "README"), "hello")
    tar_file = os.path.join(temp_dir, "foo-1.0.tar.gz")
    subprocess.check_call(["tar", "-cf", tar_file, "foo-1.0"],
                          cwd=temp_dir)

    dest_dir = self.MakeTempDir()
    tree = dirtree.TarballTree(tar_file)
    tree.WriteTree(cmd_env.BasicEnv(), dest_dir)
    self.assertEquals(os.listdir(dest_dir), ["README"])

    dest_dir = self.MakeTempDir()
    tree = dirtree.MultiTarballTree([tar_file, tar_file])
    tree.WriteTree(cmd_env.BasicEnv(), dest_dir)
    self.assertEquals(os.listdir(dest_dir), ["README"])


if __name__ == "__main__":
  unittest.main()
