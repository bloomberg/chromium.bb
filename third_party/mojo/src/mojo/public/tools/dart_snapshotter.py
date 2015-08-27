#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import sys
import os

def main():
  parser = argparse.ArgumentParser(description='Snapshots Mojo applications')
  parser.add_argument('executable', type=str)
  parser.add_argument('main', type=str)
  parser.add_argument('--package-root', type=str)
  parser.add_argument('--snapshot', type=str)

  args = parser.parse_args()
  if not os.path.isfile(args.executable):
    print "file not found: " + args.executable
  return subprocess.check_call([
    args.executable,
    args.main,
    '--package-root=%s' % args.package_root,
    '--snapshot=%s' % args.snapshot,
  ])

if __name__ == '__main__':
  sys.exit(main())
