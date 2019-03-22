#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download llvm-strip from google storage."""

import os
import sys

import update

LLVM_BUILD_DIR = update.LLVM_BUILD_DIR
STRIP_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'llvm-strip')
STAMP_FILE = os.path.normpath(
    os.path.join(LLVM_BUILD_DIR, 'llvmstrip_build_revision'))


def AlreadyUpToDate():
  if not os.path.exists(STRIP_PATH):
    return False
  stamp = update.ReadStampFile(STAMP_FILE)
  return stamp.rstrip() == update.PACKAGE_VERSION


def main():
  if not AlreadyUpToDate():
    cds_file = 'llvmstrip-%s.tgz' % update.PACKAGE_VERSION
    cds_full_url = update.GetPlatformUrlPrefix(sys.platform) + cds_file
    update.DownloadAndUnpack(cds_full_url, update.LLVM_BUILD_DIR)
  return 0


if __name__ == '__main__':
  sys.exit(main())
