# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import subprocess
import sys


BASEDIR = os.path.dirname(os.path.abspath(__file__))


def CalculateHash(root):
  """Calculates the sha1 of the paths to all files in the given |root| and the
  contents of those files, and returns as a hex string."""
  assert not os.path.isabs(root)
  assert os.path.normpath(root) == root
  digest = hashlib.sha1()
  count = 0
  for root, dirs, files in os.walk(root):
    dirs.sort()
    for name in sorted(f.lower() for f in files):
      path = os.path.join(root, name)
      digest.update(path.lower())
      with open(path, 'rb') as f:
        digest.update(f.read())
  return digest.hexdigest()


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
  # Note that this script is only run when a .sha1 file is updated (per DEPS)
  # so this relatively expensive step of hashing everything only happens when
  # the toolchain is updated.
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
  return 0


if __name__ == '__main__':
  sys.exit(main())
