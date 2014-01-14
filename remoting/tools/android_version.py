#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts a dotted-quad version string to a single numeric value suitable for
an Android package's internal version number."""

import sys

def main():
  if len(sys.argv) != 2:
    print "Usage: %s version-string" % sys.argv[0]
    exit(1)

  version_string = sys.argv[1]
  version_components = version_string.split('.')
  if len(version_components) != 4:
    print "Expected 4 components."
    exit(1)

  branch = int(version_components[2])
  patch = int(version_components[3])
  print branch * 1000 + patch


if __name__ == '__main__':
  main()
