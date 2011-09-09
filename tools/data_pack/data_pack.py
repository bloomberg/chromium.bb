#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple utility function to produce data pack files.
See base/pack_file* for details.

TOOD(adriansc): Remove this file once the dependency has been updated in WebKit
to point to grit scripts.
"""

import struct

FILE_FORMAT_VERSION = 3
HEADER_LENGTH = 2 * 4  # Two uint32s. (file version and number of entries)

class WrongFileVersion(Exception):
  pass

def ReadDataPack(input_file):
  """Reads a data pack file and returns a dictionary."""
  data = open(input_file, "rb").read()
  original_data = data

  # Read the header.
  version, num_entries = struct.unpack("<II", data[:HEADER_LENGTH])
  if version != FILE_FORMAT_VERSION:
    raise WrongFileVersion

  resources = {}
  if num_entries == 0:
    return resources

  # Read the index and data.
  data = data[HEADER_LENGTH:]
  kIndexEntrySize = 2 + 4  # Each entry is a uint16 and a uint32.
  for _ in range(num_entries):
    id, offset = struct.unpack("<HI", data[:kIndexEntrySize])
    data = data[kIndexEntrySize:]
    next_id, next_offset = struct.unpack("<HI", data[:kIndexEntrySize])
    resources[id] = original_data[offset:next_offset]

  return resources

def WriteDataPack(resources, output_file):
  """Write a map of id=>data into output_file as a data pack."""
  ids = sorted(resources.keys())
  file = open(output_file, "wb")

  # Write file header.
  file.write(struct.pack("<II", FILE_FORMAT_VERSION, len(ids)))

  # Each entry is a uint16 and a uint32. We have one extra entry for the last
  # item.
  index_length = (len(ids) + 1) * (2 + 4)

  # Write index.
  data_offset = HEADER_LENGTH + index_length
  for id in ids:
    file.write(struct.pack("<HI", id, data_offset))
    data_offset += len(resources[id])

  file.write(struct.pack("<HI", 0, data_offset))

  # Write data.
  for id in ids:
    file.write(resources[id])

def main():
  # Just write a simple file.
  data = { 1: "", 4: "this is id 4", 6: "this is id 6", 10: "" }
  WriteDataPack(data, "datapack1.pak")
  data2 = { 1000: "test", 5: "five" }
  WriteDataPack(data2, "datapack2.pak")
  print "wrote datapack1 and datapack2 to current directory."

if __name__ == '__main__':
  main()
