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

FILE_FORMAT_VERSION = 4
HEADER_LENGTH = 2 * 4 + 1  # Two uint32s. (file version, number of entries) and
                           # one uint8 (encoding of text resources)
BINARY, UTF8, UTF16 = range(3)

class WrongFileVersion(Exception):
  pass

class DataPackContents:
  def __init__(self, resources, encoding):
    self.resources = resources
    self.encoding = encoding

def ReadDataPack(input_file):
  """Reads a data pack file and returns a dictionary."""
  data = open(input_file, "rb").read()
  original_data = data

  # Read the header.
  version, num_entries, encoding = struct.unpack("<IIB", data[:HEADER_LENGTH])
  if version != FILE_FORMAT_VERSION:
    print "Wrong file version in ", input_file
    raise WrongFileVersion

  resources = {}
  if num_entries == 0:
    return DataPackContents(resources, encoding)

  # Read the index and data.
  data = data[HEADER_LENGTH:]
  kIndexEntrySize = 2 + 4  # Each entry is a uint16 and a uint32.
  for _ in range(num_entries):
    id, offset = struct.unpack("<HI", data[:kIndexEntrySize])
    data = data[kIndexEntrySize:]
    next_id, next_offset = struct.unpack("<HI", data[:kIndexEntrySize])
    resources[id] = original_data[offset:next_offset]

  return DataPackContents(resources, encoding)

def WriteDataPack(resources, output_file, encoding):
  """Write a map of id=>data into output_file as a data pack."""
  ids = sorted(resources.keys())
  file = open(output_file, "wb")

  # Write file header.
  file.write(struct.pack("<IIB", FILE_FORMAT_VERSION, len(ids), encoding))

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
  WriteDataPack(data, "datapack1.pak", UTF8)
  data2 = { 1000: "test", 5: "five" }
  WriteDataPack(data2, "datapack2.pak", UTF8)
  print "wrote datapack1 and datapack2 to current directory."

if __name__ == '__main__':
  main()
