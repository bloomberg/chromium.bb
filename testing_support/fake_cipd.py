#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def main():
  assert sys.argv[1] == 'ensure'
  parser = argparse.ArgumentParser()
  parser.add_argument('-ensure-file')
  parser.add_argument('-root')
  args, _ = parser.parse_known_args()

  cipd_root = os.path.join(args.root, '.cipd')
  if not os.path.isdir(cipd_root):
    os.makedirs(cipd_root)

  if args.ensure_file:
    shutil.copy(args.ensure_file, os.path.join(cipd_root, 'ensure'))

  return 0


if __name__ == '__main__':
  sys.exit(main())
