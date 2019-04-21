#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import cStringIO
import gzip
import os
import re
import sys


_HERE_PATH = os.path.join(os.path.dirname(__file__))

# The name of a dummy file to be updated always after all other files have been
# written. This file is declared as the "output" for GN's purposes
_TIMESTAMP_FILENAME = os.path.join('unpack.stamp')


_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.insert(1, os.path.join(_SRC_PATH, 'tools', 'grit'))
from grit.format import data_pack

ResourceFile = collections.namedtuple('ResourceFile',
                                      ['path', 'gzipped'])

def UngzipString(data):
  # Ungzipping using Python's built in gzip.
  with gzip.GzipFile(fileobj=cStringIO.StringIO(data)) as gzip_file:
    return gzip_file.read()

def ParseLine(line):
  return re.match('  {"([^"]+)", ([^},]+)(?:, ([^},]+))?', line)


def Unpack(pak_path, out_path):
  pak_dir = os.path.dirname(pak_path)
  pak_id = os.path.splitext(os.path.basename(pak_path))[0]

  data = data_pack.ReadDataPack(pak_path)

  # Associate numerical grit IDs to strings.
  # For example 120045 -> 'IDR_SETTINGS_ABOUT_PAGE_HTML'
  resource_ids = dict()
  resources_path = os.path.join(pak_dir, 'grit', pak_id + '.h')
  with open(resources_path) as resources_file:
    for line in resources_file:
      res = re.match('^#define (\S*).* (\d+)\)?$', line)
      if res:
        resource_ids[int(res.group(2))] = res.group(1)
  assert resource_ids

  # Associate numerical string IDs to files.
  resource_files = dict()
  resources_map_path = os.path.join(pak_dir, 'grit', pak_id + '_map.cc')
  with open(resources_map_path) as resources_map:
    for line in resources_map:
      res = ParseLine(line)
      if res:
        resource_files[res.group(2)] = ResourceFile(
          path=res.group(1),
          gzipped=res.group(3) == 'true')
  assert resource_files

  # Extract packed files, while preserving directory structure.
  for (resource_id, text) in data.resources.iteritems():
    resource_file = resource_files[resource_ids[resource_id]]
    file_path = resource_file.path
    file_gzipped = resource_file.gzipped
    file_dir = os.path.join(out_path, os.path.dirname(file_path))
    if not os.path.exists(file_dir):
      os.makedirs(file_dir)
    if file_gzipped:
      text = UngzipString(text)
    with open(os.path.join(out_path, file_path), 'w') as f:
      f.write(text)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pak_file')
  parser.add_argument('--out_folder')
  args = parser.parse_args()

  Unpack(args.pak_file, args.out_folder)

  timestamp_file_path = os.path.join(args.out_folder, _TIMESTAMP_FILENAME)
  with open(timestamp_file_path, 'a'):
    os.utime(timestamp_file_path, None)


if __name__ == '__main__':
  main()
