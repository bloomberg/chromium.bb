#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import os
import shutil
import subprocess


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


def GetOne(lst):
  assert len(lst) == 1, lst
  return lst[0]


def CopyOnto(source_dir, dest_dir):
  for leafname in os.listdir(source_dir):
    subprocess.check_call(["cp", "-a", os.path.join(source_dir, leafname),
                           "-t", dest_dir])


def CopyOntoHardlink(source_dir, dest_dir):
  for leafname in os.listdir(source_dir):
    subprocess.check_call(["cp", "-a", "-l", os.path.join(source_dir, leafname),
                           "-t", dest_dir])


def RemoveTree(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def MkdirP(dir_path):
  subprocess.check_call(["mkdir", "-p", dir_path])


class DirTree(object):

  # WriteTree(dest_dir) makes a fresh copy of the tree in dest_dir.
  # It can assume that dest_dir is initially empty.
  # The state of dest_dir is undefined if WriteTree() fails.
  def WriteTree(self, dest_dir):
    raise NotImplementedError()


class EmptyTree(DirTree):

  def WriteTree(self, dest_dir):
    pass


class TarballTree(DirTree):

  def __init__(self, tar_path):
    self._tar_path = tar_path

  def WriteTree(self, dest_dir):
    # Tarballs normally contain a single top-level directory with
    # a name like foo-module-1.2.3.  We strip this off.
    assert os.listdir(dest_dir) == []
    subprocess.check_call(["tar", "-C", dest_dir, "-xf", self._tar_path])
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

  def WriteTree(self, dest_dir):
    assert os.listdir(dest_dir) == []
    for tar_file in self._tar_paths:
      subprocess.check_call(["tar", "-C", dest_dir, "-xf", tar_file])
    tar_name = GetOne(os.listdir(dest_dir))
    for leafname in os.listdir(os.path.join(dest_dir, tar_name)):
      os.rename(os.path.join(dest_dir, tar_name, leafname),
                os.path.join(dest_dir, leafname))
    os.rmdir(os.path.join(dest_dir, tar_name))


class PatchedTree(DirTree):

  def __init__(self, orig_tree, patch_files, strip=1):
    self._orig_tree = orig_tree
    self._patch_files = patch_files
    self._strip = strip

  def WriteTree(self, dest_dir):
    self._orig_tree.WriteTree(dest_dir)
    for patch_file in self._patch_files:
      subprocess.check_call(["patch", "--forward", "--batch", "-d", dest_dir,
                             "-p%i" % self._strip, "-i", patch_file])


class CopyTree(DirTree):

  def __init__(self, src_path):
    self._src_path = src_path

  def WriteTree(self, dest_path):
    CopyOnto(self._src_path, dest_path)
