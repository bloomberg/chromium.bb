#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
A simple utility function to merge data pack files into a single data pack. See
http://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings
for details about the file format.
"""

import sys

import data_pack

def main(argv):
  if len(argv) < 3:
    print ("Usage:\n  %s <output_filename> <input_file1> [input_file2] ... " %
           argv[0])
    sys.exit(-1)
  data_pack.DataPack.RePack(argv[1], argv[2:])

if '__main__' == __name__:
  main(sys.argv)
