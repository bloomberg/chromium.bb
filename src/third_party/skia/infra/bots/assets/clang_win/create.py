#!/usr/bin/env python
#
# Copyright 2017 Google Inc.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Create the asset."""


import argparse
import common
import os
import subprocess
import utils


# Copied from https://cs.chromium.org/chromium/src/tools/clang/scripts/update.py
CLANG_REVISION = 'c2443155a0fb245c8f17f2c1c72b6ea391e86e81'
CLANG_SVN_REVISION = 'n332890'
CLANG_SUB_REVISION = 1

PACKAGE_VERSION = '%s-%s-%s' % (CLANG_SVN_REVISION, CLANG_REVISION[:8],
                                CLANG_SUB_REVISION)
# (End copying)

GS_URL = ('https://commondatastorage.googleapis.com/chromium-browser-clang'
          '/Win/clang-%s.tgz' % PACKAGE_VERSION)


def create_asset(target_dir):
  """Create the asset."""
  with utils.chdir(target_dir):
    tarball = 'clang.tgz'
    subprocess.check_call(['wget', '-O', tarball, GS_URL])
    subprocess.check_call(['tar', 'zxvf', tarball])
    os.remove(tarball)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--target_dir', '-t', required=True)
  args = parser.parse_args()
  create_asset(args.target_dir)


if __name__ == '__main__':
  main()
