#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sleeps."""

import sys
import time


def main():
  assert len(sys.argv) == 2
  print 'Sleeping.'
  sys.stdout.flush()
  time.sleep(float(sys.argv[1]))
  print 'Slept.'
  return 0


if __name__ == '__main__':
  sys.exit(main())
