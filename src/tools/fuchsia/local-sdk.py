#!/usr/bin/env python

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile


SELF_FILE = os.path.normpath(os.path.abspath(__file__))
REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))


def Run(*args):
  print 'Run:', ' '.join(args)
  subprocess.check_call(args)


def EnsureEmptyDir(path):
  if os.path.isdir(path):
    shutil.rmtree(path)
  if not os.path.exists(path):
    print 'Creating directory', path
    os.makedirs(path)


def BuildForArch(arch):
  build_dir = 'out/release-' + arch
  Run('scripts/fx', 'set', arch,
      '--packages=topaz/packages/sdk/topaz',
      '--args=is_debug=false', build_dir)
  Run('scripts/fx', 'full-build')


def main(args):
  if len(args) != 1 or not os.path.isdir(args[0]):
    print 'usage: %s <path_to_fuchsia_tree>' % SELF_FILE
    return 1

  original_dir = os.getcwd()

  fuchsia_root = args[0]

  # Switch to the Fuchsia tree and build an SDK.
  os.chdir(fuchsia_root)

  BuildForArch('x64')
  BuildForArch('arm64')

  tempdir = tempfile.mkdtemp()
  sdk_tar = os.path.join(tempdir, 'fuchsia-sdk.tgz')
  Run('go', 'run', 'scripts/sdk/foundation/makesdk.go', '-output', sdk_tar, '.')

  # Nuke the SDK from DEPS, put our just-built one there, and set a fake .hash
  # file. This means that on next gclient runhooks, we'll restore to the
  # real DEPS-determined SDK.
  output_dir = os.path.join(REPOSITORY_ROOT, 'third_party', 'fuchsia-sdk',
                            'sdk')
  EnsureEmptyDir(output_dir)
  tarfile.open(sdk_tar, mode='r:gz').extractall(path=output_dir)

  print 'Hashing sysroot...'
  # Hash the sysroot to catch updates to the headers, but don't hash the whole
  # tree, as we want to avoid rebuilding all of Chromium if it's only e.g. the
  # kernel blob has changed. https://crbug.com/793956.
  sysroot_hash_obj = hashlib.sha1()
  for root, dirs, files in os.walk(os.path.join(output_dir, 'sysroot')):
    for f in files:
      path = os.path.join(root, f)
      sysroot_hash_obj.update(path)
      sysroot_hash_obj.update(open(path, 'rb').read())
  sysroot_hash = sysroot_hash_obj.hexdigest()

  hash_filename = os.path.join(output_dir, '.hash')
  with open(hash_filename, 'w') as f:
    f.write('locally-built-sdk-' + sysroot_hash)

  # Clean up.
  shutil.rmtree(tempdir)
  os.chdir(original_dir)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
