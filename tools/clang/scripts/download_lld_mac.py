#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to download lld/mac from google storage."""

import os
import re
import subprocess
import sys

import update

LLVM_BUILD_DIR = update.LLVM_BUILD_DIR
LLD_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'lld')
LD_LLD_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'ld.lld')
LLD_LINK_PATH = os.path.join(LLVM_BUILD_DIR, 'bin', 'lld-link')


def AlreadyUpToDate():
  if not os.path.exists(LLD_PATH):
    return False
  # lld-link seems to have no flag to query the version; go to ld.lld instead.
  # TODO(thakis): Remove ld.lld workaround if https://llvm.org/PR34955 is fixed.
  lld_rev = subprocess.check_output([LD_LLD_PATH, '--version'])
  return (re.match(r'LLD.*\(trunk (\d+)\)', lld_rev).group(1) !=
             update.CLANG_REVISION)


def main():
  if AlreadyUpToDate():
    return 0

  remote_path = '%s/Mac/lld-%s.tgz' % (update.CDS_URL, update.PACKAGE_VERSION)
  update.DownloadAndUnpack(remote_path, update.LLVM_BUILD_DIR)

  # TODO(thakis): Remove this once the lld tgz includes the symlink.
  if os.path.exists(LLD_LINK_PATH):
    os.remove(LLD_LINK_PATH)
    os.remove(LD_LLD_PATH)
  os.symlink('lld', LLD_LINK_PATH)
  os.symlink('lld', LD_LLD_PATH)
  return 0

if __name__ == '__main__':
  sys.exit(main())
