#!/usr/bin/python

# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import hashlib
import os
import shutil
import stat
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


def HashFile(filename):
  hasher = hashlib.sha1()
  fh = open(filename, "r")
  try:
    while True:
      data = fh.read(4096)
      if len(data) == 0:
        break
      hasher.update(data)
  finally:
    fh.close()
  return hasher.hexdigest()


def GetOne(lst):
  assert len(lst) == 1, lst
  return lst[0]


def CopyOnto(source_dir, dest_dir):
  # This assertion is a replacment for the "-t" option, which is in
  # the GNU tools but not in the BSD tools on Mac OS X.
  assert os.path.isdir(dest_dir)
  for leafname in os.listdir(source_dir):
    subprocess.check_call(["cp", "-pR", os.path.join(source_dir, leafname),
                           dest_dir])


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


# A tree snapshot represents a directory tree.  Directories are
# represented as dictionaries, with FileSnapshot objects as the
# leaves.  A FileSnapshot object represents an immutable file.
#
# Tree snapshots are useful for transforming directories in a
# mostly-functional way.
#
# This structure means we copy the file listing into memory, but not
# the files themselves.  This is OK because the trees we deal with do
# not contain huge numbers of files.

class FileSnapshot(object):

  def IsSymlink(self):
    """
    If this returns True, this leaf node is a symlink, and
    GetSymlinkDest() is defined.  Otherwise, this is a file, and the
    other getter methods are defined.
    """
    return False

  def CopyToPath(self, dest_path):
    raise NotImplementedError()

  def GetContents(self):
    raise NotImplementedError()

  def GetHash(self):
    raise NotImplementedError()

  def IsExecutable(self):
    raise NotImplementedError()

  def GetSymlinkDest(self):
    raise NotImplementedError()


class SymlinkSnapshot(FileSnapshot):

  def __init__(self, symlink_dest):
    self._symlink_dest = symlink_dest

  def CopyToPath(self, dest_path):
    os.symlink(self._symlink_dest, dest_path)

  def IsSymlink(self):
    return True

  def GetSymlinkDest(self):
    return self._symlink_dest


class FileSnapshotUsingHardLink(FileSnapshot):

  def __init__(self, filename, stat_info, file_hasher):
    self._filename = filename
    self._stat_info = stat_info
    self._file_hasher = file_hasher

  def CopyToPath(self, dest_path):
    os.link(self._filename, dest_path)

  def GetContents(self):
    return ReadFile(self._filename)

  def GetHash(self):
    return self._file_hasher.HashFileGivenStat(self._filename, self._stat_info)

  def IsExecutable(self):
    return self._stat_info.st_mode & stat.S_IXUSR != 0


class FileSnapshotInMemory(FileSnapshot):

  def __init__(self, data, executable=False):
    self._data = data
    self._is_executable = executable

  def CopyToPath(self, dest_path):
    if self._is_executable:
      mode = 0777
    else:
      mode = 0666
    fd = os.open(dest_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, mode)
    fh = os.fdopen(fd, "w")
    try:
      fh.write(self._data)
    finally:
      fh.close()

  def GetContents(self):
    return self._data

  def GetHash(self):
    return hashlib.sha1(self._data).hexdigest()

  def IsExecutable(self):
    return self._is_executable


class LazyDict(object):

  """
  LazyDict(func) returns a read-only dictionary object whose contents
  are lazily evaluated by calling func(), which is expected to return
  a dictionary.  LazyDict implements a read-only subset of the
  interface of dict objects.
  """

  def __init__(self, func):
    self._func = func
    self._dict = None

  def _GetDict(self):
    if self._dict is None:
      self._dict = self._func()
      self._func = None
    return self._dict

  def iteritems(self):
    return self._GetDict().iteritems()

  def copy(self):
    return self._GetDict().copy()

  def __getitem__(self, key):
    return self._GetDict()[key]

  def __contains__(self, key):
    return key in self._GetDict()

  def get(self, *args):
    return self._GetDict().get(*args)


def MakeSnapshotFromPath(filename, file_hasher):
  st = os.lstat(filename)
  if stat.S_ISREG(st.st_mode):
    return FileSnapshotUsingHardLink(filename, st, file_hasher)
  elif stat.S_ISLNK(st.st_mode):
    return SymlinkSnapshot(os.readlink(filename))
  elif stat.S_ISDIR(st.st_mode):
    def ReadDir():
      return dict(
          (leafname, MakeSnapshotFromPath(os.path.join(filename, leafname),
                                          file_hasher))
          for leafname in os.listdir(filename))
    return LazyDict(ReadDir)
  else:
    raise AssertionError("Unknown file type 0%o: %r" % (st.st_mode, filename))


def WriteSnapshotToPath(snapshot, dest_path):
  if isinstance(snapshot, FileSnapshot):
    snapshot.CopyToPath(dest_path)
  else:
    if not os.path.exists(dest_path):
      os.mkdir(dest_path)
    for leafname, entry in snapshot.iteritems():
      WriteSnapshotToPath(entry, os.path.join(dest_path, leafname))
