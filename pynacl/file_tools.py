#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convience stand-alone file operations."""


import os
import shutil
import sys
import tempfile

import platform
import time


def AtomicWriteFile(data, filename):
  """Write a file atomically.

  NOTE: Not atomic on Windows!
  Args:
    data: String to write to the file.
    filename: Filename to write.
  """
  filename = os.path.abspath(filename)
  handle, temp_file = tempfile.mkstemp(
      prefix='atomic_write', suffix='.tmp',
      dir=os.path.dirname(filename))
  fh = os.fdopen(handle, 'wb')
  fh.write(data)
  fh.close()
  # Window's can't move into place atomically, delete first.
  if sys.platform in ['win32', 'cygwin']:
    try:
      os.remove(filename)
    except OSError:
      pass
  os.rename(temp_file, filename)


def WriteFile(data, filename):
  """Write a file in one step.

  Args:
    data: String to write to the file.
    filename: Filename to write.
  """
  fh = open(filename, 'wb')
  fh.write(data)
  fh.close()


def ReadFile(filename):
  """Read a file in one step.

  Args:
    filename: Filename to read.
  Returns:
    String containing complete file.
  """
  fh = open(filename, 'rb')
  data = fh.read()
  fh.close()
  return data


class ExecutableNotFound(Exception):
  pass


def Which(command, paths=None, require_executable=True):
  """Find the absolute path of a command in the current PATH.

  Args:
    command: Command name to look for.
    paths: Optional paths to search.
  Returns:
    Absolute path of the command (first one found),
    or default to a bare command if nothing is found.
  """
  if paths is None:
    paths = os.environ.get('PATH', '').split(os.pathsep)
  exe_suffixes = ['']
  if sys.platform == 'win32':
    exe_suffixes += ['.exe']
  for p in paths:
    np = os.path.abspath(os.path.join(p, command))
    for suffix in exe_suffixes:
      full_path = np + suffix
      if (os.path.isfile(full_path) and
          (not require_executable or os.access(full_path, os.X_OK))):
        return full_path
  raise ExecutableNotFound('Unable to find: ' + command)


def MakeDirectoryIfAbsent(path):
  """Create a directory if it doesn't already exist.

  Args:
    path: Directory to create.
  """
  if not os.path.exists(path):
    os.mkdir(path)


def RemoveDirectoryIfPresent(path):
  """Remove a directory if it exists.

  Args:
    path: Directory to remove.
  """

  # On Windows, attempts to remove read-only files get Error 5. This
  # error handler fixes the permissions and retries the removal.
  def onerror_readonly(func, path, exc_info):
    import stat
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      func(path)

  if os.path.exists(path):
    shutil.rmtree(path, onerror=onerror_readonly)


def CopyTree(src, dst):
  """Recursively copy the items in the src directory to the dst directory.

  Unlike shutil.copytree, the destination directory and any subdirectories and
  files may exist. Existing directories are left untouched, and existing files
  are removed and copied from the source using shutil.copy2. It is also not
  symlink-aware.

  Args:
    src: Source. Must be an existing directory.
    dst: Destination directory. If it exists, must be a directory. Otherwise it
         will be created, along with parent directories.
  """
  if not os.path.isdir(dst):
    os.makedirs(dst)
  for root, dirs, files in os.walk(src):
    relroot = os.path.relpath(root, src)
    dstroot = os.path.join(dst, relroot)
    for d in dirs:
      dstdir = os.path.join(dstroot, d)
      if not os.path.isdir(dstdir):
        os.mkdir(dstdir)
    for f in files:
      dstfile = os.path.join(dstroot, f)
      if os.path.isfile(dstfile):
        os.remove(dstfile)
      shutil.copy2(os.path.join(root, f), dstfile)


def Retry(op, *args):
  # Windows seems to be prone to having commands that delete files or
  # directories fail.  We currently do not have a complete understanding why,
  # and as a workaround we simply retry the command a few times.
  # It appears that file locks are hanging around longer than they should.  This
  # may be a secondary effect of processes hanging around longer than they
  # should.  This may be because when we kill a browser sel_ldr does not exit
  # immediately, etc.
  # Virus checkers can also accidently prevent files from being deleted, but
  # that shouldn't be a problem on the bots.
  if platform.IsWindows():
    count = 0
    while True:
      try:
        op(*args)
        break
      except Exception:
        sys.stdout.write('FAILED: %s %s\n' % (op.__name__, repr(args)))
        count += 1
        if count < 5:
          sys.stdout.write('RETRY: %s %s\n' % (op.__name__, repr(args)))
          time.sleep(pow(2, count))
        else:
          # Don't mask the exception.
          raise
  else:
    op(*args)


def MoveDirCleanly(src, dst):
  RemoveDir(dst)
  MoveDir(src, dst)


def MoveDir(src, dst):
  Retry(shutil.move, src, dst)


def RemoveDir(path):
  if os.path.exists(path):
    Retry(shutil.rmtree, path)


def RemoveFile(path):
  if os.path.exists(path):
    Retry(os.unlink, path)
