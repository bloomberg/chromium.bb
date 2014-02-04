#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads and unpacks a toolchain for building on Windows. The contents are
matched by sha1 which will be updated when the toolchain is updated.

Having a toolchain script in depot_tools means that it's not versioned
directly with the source code. That is, if the toolchain is upgraded, but
you're trying to build an historical version of Chromium from before the
toolchain upgrade, this will cause you to build with a newer toolchain than
was available when that code was committed. This is done for a two main
reasons: 1) it would likely be annoying to have the up-to-date toolchain
removed and replaced by one without a service pack applied); 2) it would
require maintaining scripts that can build older not-up-to-date revisions of
the toolchain. This is likely to be a poorly tested code path that probably
won't be properly maintained. See http://crbug.com/323300.

This does not extend to major versions of the toolchain however, on the
assumption that there are more likely to be source incompatibilities between
major revisions. This script calls a subscript (currently, toolchain2013.py)
to do the main work. It is expected that toolchain2013.py will always be able
to acquire/build the most current revision of a VS2013-based toolchain. In the
future when a hypothetical VS2015 is released, the 2013 script will be
maintained, and a new 2015 script would be added.
"""

import ctypes.wintypes
import hashlib
import json
import os
import subprocess
import sys


BASEDIR = os.path.dirname(os.path.abspath(__file__))


GetFileAttributes = ctypes.windll.kernel32.GetFileAttributesW
GetFileAttributes.argtypes = (ctypes.wintypes.LPWSTR,)
GetFileAttributes.restype = ctypes.wintypes.DWORD
FILE_ATTRIBUTE_HIDDEN = 0x2
FILE_ATTRIBUTE_SYSTEM = 0x4


def IsHidden(file_path):
  """Returns whether the given |file_path| has the 'system' or 'hidden'
  attribute set."""
  p = GetFileAttributes(file_path)
  assert p != 0xffffffff
  return bool(p & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))


def GetFileList(root):
  """Gets a normalized list of files under |root|."""
  assert not os.path.isabs(root)
  assert os.path.normpath(root) == root
  file_list = []
  for base, _, files in os.walk(root):
    paths = [os.path.join(base, f) for f in files]
    file_list.extend(x.lower() for x in paths if not IsHidden(x))
  return sorted(file_list)


def MakeTimestampsFileName(root):
  return os.path.join(root, '..', '.timestamps')


def CalculateHash(root):
  """Calculates the sha1 of the paths to all files in the given |root| and the
  contents of those files, and returns as a hex string."""
  file_list = GetFileList(root)

  # Check whether we previously saved timestamps in $root/../.timestamps. If
  # we didn't, or they don't match, then do the full calculation, otherwise
  # return the saved value.
  timestamps_file = MakeTimestampsFileName(root)
  timestamps_data = {'files': [], 'sha1': ''}
  if os.path.exists(timestamps_file):
    with open(timestamps_file, 'rb') as f:
      try:
        timestamps_data = json.load(f)
      except ValueError:
        # json couldn't be loaded, empty data will force a re-hash.
        pass

  matches = len(file_list) == len(timestamps_data['files'])
  if matches:
    for disk, cached in zip(file_list, timestamps_data['files']):
      if disk != cached[0] or os.stat(disk).st_mtime != cached[1]:
        matches = False
        break
  if matches:
    return timestamps_data['sha1']

  digest = hashlib.sha1()
  for path in file_list:
    digest.update(path)
    with open(path, 'rb') as f:
      digest.update(f.read())
  return digest.hexdigest()


def SaveTimestampsAndHash(root, sha1):
  """Saves timestamps and the final hash to be able to early-out more quickly
  next time."""
  file_list = GetFileList(root)
  timestamps_data = {
    'files': [[f, os.stat(f).st_mtime] for f in file_list],
    'sha1': sha1,
  }
  with open(MakeTimestampsFileName(root), 'wb') as f:
    json.dump(timestamps_data, f)


def HaveSrcInternalAccess():
  """Checks whether access to src-internal is available."""
  with open(os.devnull, 'w') as nul:
    if subprocess.call(
        ['svn', 'ls',
         'svn://svn.chromium.org/chrome-internal/trunk/src-internal/'],
        shell=True, stdin=nul, stdout=nul, stderr=nul) == 0:
      return True
    return subprocess.call(
        ['git', 'remote', 'show',
         'https://chrome-internal.googlesource.com/chrome/src-internal/'],
        shell=True, stdin=nul, stdout=nul, stderr=nul) == 0


def main():
  if not sys.platform.startswith(('cygwin', 'win32')):
    return 0

  if len(sys.argv) != 1:
    print >> sys.stderr, 'Unexpected arguments.'
    return 1

  # Move to depot_tools\win_toolchain where we'll store our files, and where
  # the downloader script is.
  os.chdir(os.path.normpath(os.path.join(BASEDIR)))
  toolchain_dir = '.'
  target_dir = os.path.normpath(os.path.join(toolchain_dir, 'vs2013_files'))

  sha1path = os.path.join(toolchain_dir, 'toolchain_vs2013.hash')
  desired_hashes = set()
  if os.path.isfile(sha1path):
    with open(sha1path, 'rb') as f:
      desired_hashes = set(f.read().strip().splitlines())

  # If the current hash doesn't match what we want in the file, nuke and pave.
  # Typically this script is only run when the .sha1 one file is updated, but
  # directly calling "gclient runhooks" will also run it, so we cache
  # based on timestamps to make that case fast.
  current_hash = CalculateHash(target_dir)
  if current_hash not in desired_hashes:
    should_get_pro = (os.path.isfile(os.path.join(BASEDIR, '.vspro')) or
                      HaveSrcInternalAccess())
    print('Windows toolchain out of date or doesn\'t exist, updating (%s)...' %
          ('Pro' if should_get_pro else 'Express'))
    # This stays resident and will make the rmdir below fail.
    subprocess.call(['taskkill', '/f', '/im', 'mspdbsrv.exe'])
    if os.path.isdir(target_dir):
      subprocess.check_call('rmdir /s/q "%s"' % target_dir, shell=True)
    args = [sys.executable,
            'toolchain2013.py',
            '--targetdir', target_dir]
    if not should_get_pro:
      args.append('--express')
    subprocess.check_call(args)
    current_hash = CalculateHash(target_dir)
    if current_hash not in desired_hashes:
      print >> sys.stderr, (
          'Got wrong hash after pulling a new toolchain. '
          'Wanted one of \'%s\', got \'%s\'.' % (
              desired_hashes, current_hash))
      return 1
    SaveTimestampsAndHash(target_dir, current_hash)

  return 0


if __name__ == '__main__':
  sys.exit(main())
