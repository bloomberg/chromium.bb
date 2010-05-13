#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os


def GetOne(lst):
  assert len(lst) == 1, lst
  return lst[0]


class DirTree(object):

  # WriteTree(dest_dir) makes a fresh copy of the tree in dest_dir.
  # It can assume that dest_dir is initially empty.
  # The state of dest_dir is undefined if WriteTree() fails.
  def WriteTree(self, env, dest_dir):
    raise NotImplementedError()


class EmptyTree(DirTree):

  def WriteTree(self, env, dest_dir):
    pass


class TarballTree(DirTree):

  def __init__(self, tar_path):
    self._tar_path = tar_path

  def WriteTree(self, env, dest_dir):
    # Tarballs normally contain a single top-level directory with
    # a name like foo-module-1.2.3.  We strip this off.
    assert os.listdir(dest_dir) == []
    env.cmd(["tar", "-C", dest_dir, "-xf", self._tar_path])
    tar_name = GetOne(os.listdir(dest_dir))
    for leafname in os.listdir(os.path.join(dest_dir, tar_name)):
      os.rename(os.path.join(dest_dir, tar_name, leafname),
                os.path.join(dest_dir, leafname))
    os.rmdir(os.path.join(dest_dir, tar_name))


# This handles gcc, where two source tarballs must be unpacked on top
# of each other.
class MultiTarballTree(DirTree):

  def __init__(self, tar_paths):
    self._tar_paths = tar_paths

  def WriteTree(self, env, dest_dir):
    assert os.listdir(dest_dir) == []
    for tar_file in self._tar_paths:
      env.cmd(["tar", "-C", dest_dir, "-xf", tar_file])
    tar_name = GetOne(os.listdir(dest_dir))
    for leafname in os.listdir(os.path.join(dest_dir, tar_name)):
      os.rename(os.path.join(dest_dir, tar_name, leafname),
                os.path.join(dest_dir, leafname))
    os.rmdir(os.path.join(dest_dir, tar_name))


class PatchedTree(DirTree):

  def __init__(self, orig_tree, patch_files):
    self._orig_tree = orig_tree
    self._patch_files = patch_files

  def WriteTree(self, env, dest_dir):
    self._orig_tree.WriteTree(env, dest_dir)
    for patch_file in self._patch_files:
      env.cmd(["patch", "-d", dest_dir, "-p1", "-i", patch_file])
