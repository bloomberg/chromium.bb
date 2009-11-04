#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to generate file lists for idl.gyp."""

import os.path
import sys
import types


# Read in the manifest files (which are just really simple python files),
# and scrape out the file lists.
def main(argv):
  idl_list_filename = os.path.join('..', 'idl_list.manifest')
  files = eval(open(idl_list_filename, "r").read())
  files = [os.path.basename(f) for f in files]
  files.sort()
  for file in files:
    # gyp wants paths with slashes, not backslashes.
    print file.replace("\\", "/")


if __name__ == "__main__":
  main(sys.argv[1:])
