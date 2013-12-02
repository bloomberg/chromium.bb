# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
  """Save timestamps and the final hash to be able to early-out more quickly
  next time."""
  file_list = GetFileList(root)
  timestamps_data = {
    'files': [[f, os.stat(f).st_mtime] for f in file_list],
    'sha1': sha1,
  }
  with open(MakeTimestampsFileName(root), 'wb') as f:
    json.dump(timestamps_data, f)


def main():
  if sys.platform not in ('win32', 'cygwin'):
    return 0

  if len(sys.argv) != 1:
    print >> sys.stderr, 'Unexpected arguments.'
    return 1

  # Move to same location as .gclient. This is a no-op when run via gclient.
  os.chdir(os.path.normpath(os.path.join(BASEDIR, '..\\..\\..\\..')))
  toolchain_dir = 'src\\third_party\\win_toolchain'
  target_dir = os.path.join(toolchain_dir, 'files')

  sha1path = os.path.join(toolchain_dir, 'toolchain.sha1')
  desired_hash = ''
  if os.path.isfile(sha1path):
    with open(sha1path, 'rb') as f:
      desired_hash = f.read().strip()

  # If the current hash doesn't match what we want in the file, nuke and pave.
  # Typically this script is only run when the .sha1 one file is updated, but
  # directly calling "gclient runhooks" will also run it, so we cache
  # based on timestamps to make that case fast.
  current_hash = CalculateHash(target_dir)
  if current_hash != desired_hash:
    print 'Windows toolchain out of date or doesn\'t exist, updating...'
    if os.path.isdir(target_dir):
      subprocess.check_call('rmdir /s/q "%s"' % target_dir, shell=True)
    subprocess.check_call([
        sys.executable,
        'src\\tools\\win\\toolchain\\toolchain2013.py',
        '--targetdir', target_dir])
    current_hash = CalculateHash(target_dir)
    if current_hash != desired_hash:
      print >> sys.stderr, (
          'Got wrong hash after pulling a new toolchain. '
          'Wanted \'%s\', got \'%s\'.' % (
              desired_hash, current_hash))
      return 1
    SaveTimestampsAndHash(target_dir, current_hash)

  return 0


if __name__ == '__main__':
  sys.exit(main())
