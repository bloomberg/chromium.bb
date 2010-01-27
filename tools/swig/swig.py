#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around swig.

Sets the SWIG_LIB environment var to point to Lib dir
and defers control to the platform-specific swig binary.

Depends on swig binaries being available at ../../third_party/swig.
"""

import os
import sys


def main():
  swig_dir = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]),
                             os.pardir, os.pardir, 'third_party', 'swig'))
  lib_dir = os.path.join(swig_dir, "Lib")
  os.putenv("SWIG_LIB", lib_dir)
  dir_map = {
      'darwin': 'mac',
      'linux2': 'linux',
      'win32':  'win',
      'cygwin': 'win',
  }
  swig_bin = os.path.join(swig_dir, dir_map[sys.platform], 'swig')
  os.execv(swig_bin, [swig_bin] + sys.argv[1:])


if __name__ == "__main__":
  main()

