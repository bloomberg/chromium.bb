#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a dartx archive.
"""

import argparse
import os
import sys
import zipfile

def main():
  parser = argparse.ArgumentParser(description='dartx packager.')
  parser.add_argument('--snapshot',
                      help='Snapshot file.',
                      type=str)
  parser.add_argument('--output',
                      help='Path to output archive.',
                      type=str)
  arguments = parser.parse_args()

  snapshot = arguments.snapshot
  output = arguments.output

  with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as outfile:
    outfile.write(snapshot, 'snapshot_blob.bin')

  return 0

if __name__ == '__main__':
  sys.exit(main())
