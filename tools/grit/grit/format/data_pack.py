#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support for formatting a data pack file used for platform agnostic resource
files.
"""

import collections
import exceptions
import os
import struct
import sys
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from grit import util
from grit.node import include
from grit.node import message
from grit.node import structure


PACK_FILE_VERSION = 5
BINARY, UTF8, UTF16 = range(3)


class WrongFileVersion(Exception):
  pass


class CorruptDataPack(Exception):
  pass


DataPackContents = collections.namedtuple(
    'DataPackContents', 'resources encoding')


def Format(root, lang='en', output_dir='.'):
  """Writes out the data pack file format (platform agnostic resource file)."""
  data = {}
  root.info = []
  for node in root.ActiveDescendants():
    with node:
      if isinstance(node, (include.IncludeNode, message.MessageNode,
                           structure.StructureNode)):
        id, value = node.GetDataPackPair(lang, UTF8)
        if value is not None:
          data[id] = value
          root.info.append(
              '{},{},{}'.format(node.attrs.get('name'), id, node.source))
  return WriteDataPackToString(data, UTF8)


def ReadDataPack(input_file):
  return ReadDataPackFromString(util.ReadFile(input_file, util.BINARY))


def ReadDataPackFromString(data):
  """Reads a data pack file and returns a dictionary."""
  original_data = data

  # Read the header.
  version = struct.unpack('<I', data[:4])[0]
  if version == 4:
    resource_count, encoding = struct.unpack('<IB', data[4:9])
    alias_count = 0
    data = data[9:]
  elif version == 5:
    encoding, resource_count, alias_count = struct.unpack('<BxxxHH', data[4:12])
    data = data[12:]
  else:
    raise WrongFileVersion('Found version: ' + str(version))

  resources = {}
  kIndexEntrySize = 2 + 4  # Each entry is a uint16 and a uint32.
  def entry_at_index(idx):
    offset = idx * kIndexEntrySize
    return struct.unpack('<HI', data[offset:offset + kIndexEntrySize])

  prev_resource_id, prev_offset = entry_at_index(0)
  for i in xrange(1, resource_count + 1):
    resource_id, offset = entry_at_index(i)
    resources[prev_resource_id] = original_data[prev_offset:offset]
    prev_resource_id, prev_offset = resource_id, offset

  # Read the alias table.
  alias_data = data[(resource_count + 1) * kIndexEntrySize:]
  kAliasEntrySize = 2 + 2  # uint16, uint16
  def alias_at_index(idx):
    offset = idx * kAliasEntrySize
    return struct.unpack('<HH', alias_data[offset:offset + kAliasEntrySize])

  for i in xrange(alias_count):
    resource_id, index = alias_at_index(i)
    aliased_id = entry_at_index(index)[0]
    resources[resource_id] = resources[aliased_id]

  return DataPackContents(resources, encoding)


def WriteDataPackToString(resources, encoding):
  """Returns a string with a map of id=>data in the data pack format."""
  ret = []

  # Compute alias map.
  resource_ids = sorted(resources)
  # Use reversed() so that for duplicates lower IDs clobber higher ones.
  id_by_data = {resources[k]: k for k in reversed(resource_ids)}
  # Map of resource_id -> resource_id, where value < key.
  alias_map = {k: id_by_data[v] for k, v in resources.iteritems()
               if id_by_data[v] != k}

  # Write file header.
  resource_count = len(resources) - len(alias_map)
  # Padding bytes added for alignment.
  ret.append(struct.pack('<IBxxxHH', PACK_FILE_VERSION, encoding,
                         resource_count, len(alias_map)))
  HEADER_LENGTH = 4 + 4 + 2 + 2

  # Each main table entry is: uint16 + uint32 (and an extra entry at the end).
  # Each alias table entry is: uint16 + uint16.
  data_offset = HEADER_LENGTH + (resource_count + 1) * 6 + len(alias_map) * 4

  # Write main table.
  index_by_id = {}
  deduped_data = []
  index = 0
  for resource_id in resource_ids:
    if resource_id in alias_map:
      continue
    data = resources[resource_id]
    index_by_id[resource_id] = index
    ret.append(struct.pack('<HI', resource_id, data_offset))
    data_offset += len(data)
    deduped_data.append(data)
    index += 1

  assert index == resource_count
  # Add an extra entry at the end.
  ret.append(struct.pack('<HI', 0, data_offset))

  # Write alias table.
  for resource_id in sorted(alias_map):
    index = index_by_id[alias_map[resource_id]]
    ret.append(struct.pack('<HH', resource_id, index))

  # Write data.
  ret.extend(deduped_data)
  return ''.join(ret)


def WriteDataPack(resources, output_file, encoding):
  """Writes a map of id=>data into output_file as a data pack."""
  content = WriteDataPackToString(resources, encoding)
  with open(output_file, 'wb') as file:
    file.write(content)


def RePack(output_file, input_files, whitelist_file=None,
           suppress_removed_key_output=False):
  """Write a new data pack file by combining input pack files.

  Args:
      output_file: path to the new data pack file.
      input_files: a list of paths to the data pack files to combine.
      whitelist_file: path to the file that contains the list of resource IDs
                      that should be kept in the output file or None to include
                      all resources.
      suppress_removed_key_output: allows the caller to suppress the output from
                                   RePackFromDataPackStrings.

  Raises:
      KeyError: if there are duplicate keys or resource encoding is
      inconsistent.
  """
  input_data_packs = [ReadDataPack(filename) for filename in input_files]
  input_info_files = [filename + '.info' for filename in input_files]
  whitelist = None
  if whitelist_file:
    whitelist = util.ReadFile(whitelist_file, util.RAW_TEXT).strip().split('\n')
    whitelist = set(map(int, whitelist))
  resources, encoding = RePackFromDataPackStrings(
      input_data_packs, whitelist, suppress_removed_key_output)
  WriteDataPack(resources, output_file, encoding)
  with open(output_file + '.info', 'w') as output_info_file:
    for filename in input_info_files:
      with open(filename, 'r') as info_file:
        output_info_file.writelines(info_file.readlines())


def RePackFromDataPackStrings(inputs, whitelist,
                              suppress_removed_key_output=False):
  """Returns a data pack string that combines the resources from inputs.

  Args:
      inputs: a list of data pack strings that need to be combined.
      whitelist: a list of resource IDs that should be kept in the output string
                 or None to include all resources.
      suppress_removed_key_output: Do not print removed keys.

  Returns:
      DataPackContents: a tuple containing the new combined data pack and its
                        encoding.

  Raises:
      KeyError: if there are duplicate keys or resource encoding is
      inconsistent.
  """
  resources = {}
  encoding = None
  for content in inputs:
    # Make sure we have no dups.
    duplicate_keys = set(content.resources.keys()) & set(resources.keys())
    if duplicate_keys:
      raise exceptions.KeyError('Duplicate keys: ' + str(list(duplicate_keys)))

    # Make sure encoding is consistent.
    if encoding in (None, BINARY):
      encoding = content.encoding
    elif content.encoding not in (BINARY, encoding):
      raise exceptions.KeyError('Inconsistent encodings: ' + str(encoding) +
                                ' vs ' + str(content.encoding))

    if whitelist:
      whitelisted_resources = dict([(key, content.resources[key])
                                    for key in content.resources.keys()
                                    if key in whitelist])
      resources.update(whitelisted_resources)
      removed_keys = [key for key in content.resources.keys()
                      if key not in whitelist]
      if not suppress_removed_key_output:
        for key in removed_keys:
          print 'RePackFromDataPackStrings Removed Key:', key
    else:
      resources.update(content.resources)

  # Encoding is 0 for BINARY, 1 for UTF8 and 2 for UTF16
  if encoding is None:
    encoding = BINARY
  return DataPackContents(resources, encoding)


# Temporary hack for external programs that import data_pack.
# TODO(benrg): Remove this.
class DataPack(object):
  pass
DataPack.ReadDataPack = staticmethod(ReadDataPack)
DataPack.WriteDataPackToString = staticmethod(WriteDataPackToString)
DataPack.WriteDataPack = staticmethod(WriteDataPack)
DataPack.RePack = staticmethod(RePack)


def main():
  # Write a simple file.
  data = {1: '', 4: 'this is id 4', 6: 'this is id 6', 10: ''}
  WriteDataPack(data, 'datapack1.pak', UTF8)
  data2 = {1000: 'test', 5: 'five'}
  WriteDataPack(data2, 'datapack2.pak', UTF8)
  print 'wrote datapack1 and datapack2 to current directory.'


if __name__ == '__main__':
  main()
