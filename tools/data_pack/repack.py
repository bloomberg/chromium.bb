#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple utility function to merge data pack files into a single data pack.
See base/pack_file* for details about the file format.

TODO(adriansc): Remove this file once the dependency has been updated in WebKit
to point to grit scripts.
"""

import exceptions
import struct
import sys

import data_pack

def RePack(output_file, input_files):
  """Write a new data pack to |output_file| based on a list of filenames
  (|input_files|)"""
  resources = {}
  encoding = None
  for filename in input_files:
    new_content = data_pack.ReadDataPack(filename)

    # Make sure we have no dups.
    duplicate_keys = set(new_content.resources.keys()) & set(resources.keys())
    if len(duplicate_keys) != 0:
      raise exceptions.KeyError("Duplicate keys: " + str(list(duplicate_keys)))

    # Make sure encoding is consistent.
    if encoding in (None, data_pack.BINARY):
      encoding = new_content.encoding
    elif new_content.encoding not in (data_pack.BINARY, encoding):
        raise exceptions.KeyError("Inconsistent encodings: " +
                                  str(encoding) + " vs " +
                                  str(new_content.encoding))

    resources.update(new_content.resources)

  # Encoding is 0 for BINARY, 1 for UTF8 and 2 for UTF16
  if encoding is None:
    encoding = data_pack.BINARY
  data_pack.WriteDataPack(resources, output_file, encoding)

def main(argv):
  if len(argv) < 3:
    print ("Usage:\n  %s <output_filename> <input_file1> [input_file2] ... " %
           argv[0])
    sys.exit(-1)
  RePack(argv[1], argv[2:])

if '__main__' == __name__:
  main(sys.argv)
