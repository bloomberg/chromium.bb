#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cleans up a swarm slave after the tests have run."""

import glob
import os
import sys


def main():
  for filename in glob.iglob('*.zip'):
    # Try to delete as many of the zip files as possible, but don't worry about
    # errors.
    try:
      os.remove(filename)
    except OSError:
      pass


if __name__ == '__main__':
  sys.exit(main())
