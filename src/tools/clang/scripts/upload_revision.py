#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script takes a Clang git revision as an argument, it then
creates a feature branch, puts this revision into update.py, uploads
a CL, triggers Clang Upload try bots, and tells what to do next"""

from __future__ import print_function

import argparse
import fnmatch
import itertools
import os
import re
import shutil
import subprocess
import sys

from build import GetCommitCount, CheckoutLLVM, LLVM_DIR
from update import CHROMIUM_DIR

# Path constants.
THIS_DIR = os.path.dirname(__file__)
UPDATE_PY_PATH = os.path.join(THIS_DIR, "update.py")
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))

# Keep lines in here at <= 72 columns, else they wrap in gerrit.
COMMIT_FOOTER = \
'''

Cq-Include-Trybots: chromium/try:mac_chromium_asan_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_cfi_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_chromeos_asan_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_msan_rel_ng
Cq-Include-Trybots: chromium/try:linux_chromium_chromeos_msan_rel_ng
Cq-Include-Trybots: chromium/try:linux-chromeos-dbg,win-asan
Cq-Include-Trybots: chromium/try:chromeos-amd64-generic-cfi-thin-lto-rel
Cq-Include-Trybots: chromium/try:linux_chromium_compile_dbg_32_ng
Cq-Include-Trybots: chromium/try:win7-rel,win-angle-deqp-rel-32
Cq-Include-Trybots: chromium/try:win-angle-deqp-rel-64
Cq-Include-Trybots: chromium/try:dawn-win10-x86-deps-rel
Cq-Include-Trybots: chrome/try:iphone-device,ipad-device
Cq-Include-Trybots: chrome/try:linux-chromeos-chrome
Cq-Include-Trybots: chrome/try:win-chrome,win64-chrome,mac-chrome
'''

is_win = sys.platform.startswith('win32')

def PatchRevision(clang_git_revision, clang_svn_revision, clang_sub_revision):
  with open(UPDATE_PY_PATH, 'rb') as f:
    content = f.read()
  m = re.search("CLANG_REVISION = '([0-9a-f]+)'", content)
  clang_old_git_revision = m.group(1)
  m = re.search("CLANG_SVN_REVISION = '(n[0-9]+)'", content)
  clang_old_svn_revision = m.group(1)
  m = re.search("CLANG_SUB_REVISION = ([0-9]+)", content)
  clang_old_sub_revision = m.group(1)

  content = re.sub("CLANG_REVISION = '[0-9a-f]+'",
                   "CLANG_REVISION = '{}'".format(clang_git_revision),
                   content, count=1)
  content = re.sub("CLANG_SVN_REVISION = 'n[0-9]+'",
                   "CLANG_SVN_REVISION = '{}'".format(clang_svn_revision),
                   content, count=1)
  content = re.sub("CLANG_SUB_REVISION = [0-9]+",
                   "CLANG_SUB_REVISION = {}".format(clang_sub_revision),
                   content, count=1)
  with open(UPDATE_PY_PATH, 'wb') as f:
    f.write(content)
  return clang_old_git_revision, clang_old_svn_revision, clang_old_sub_revision


def Git(args):
  # Needs shell=True on Windows due to git.bat in depot_tools.
  subprocess.check_call(["git"] + args, shell=is_win)

def main():
  parser = argparse.ArgumentParser(description='upload new clang revision')
  parser.add_argument('clang_git_revision', nargs=1,
                      help='Clang git revision to build the toolchain for.')
  parser.add_argument('clang_sub_revision',
                      type=int, nargs='?', default=1,
                      help='Clang sub-revision to build the toolchain for.')

  args = parser.parse_args()

  clang_git_revision = args.clang_git_revision[0]

  # To get the commit count, we need a checkout.
  CheckoutLLVM(clang_git_revision, LLVM_DIR);
  clang_svn_revision = 'n' + GetCommitCount(clang_git_revision)
  clang_sub_revision = args.clang_sub_revision

  os.chdir(CHROMIUM_DIR)

  print("Making a patch for Clang {}-{}-{}".format(
      clang_svn_revision, clang_git_revision[:8], clang_sub_revision))

  clang_old_git_revision, clang_old_svn_revision, clang_old_sub_revision = \
      PatchRevision(clang_git_revision, clang_svn_revision, clang_sub_revision)

  rev_string = "{}-{}-{}".format(clang_svn_revision,
                                 clang_git_revision[:8],
                                 clang_sub_revision)
  Git(["checkout", "origin/master", "-b", "clang-{}".format(rev_string)])
  Git(["add", UPDATE_PY_PATH])

  old_rev_string = "{}-{}-{}".format(clang_old_svn_revision,
                                     clang_old_git_revision[:8],
                                     clang_old_sub_revision)

  commit_message = 'Ran `{}`.'.format(' '.join(sys.argv)) + COMMIT_FOOTER
  Git(["commit", "-m", "Roll clang {} : {}.\n\n{}".format(
      old_rev_string, rev_string, commit_message)])

  Git(["cl", "upload", "-f", "--bypass-hooks"])
  Git([
      "cl", "try", "-B", "chromium/try", "-b", "linux_upload_clang", "-b",
      "mac_upload_clang", "-b", "win_upload_clang"
  ])

  print ("Please, wait until the try bots succeeded "
         "and then push the binaries to goma.")

if __name__ == '__main__':
  sys.exit(main())
