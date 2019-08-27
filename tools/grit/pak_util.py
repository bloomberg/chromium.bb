#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A tool for interacting with .pak files.

For details on the pak file format, see:
https://dev.chromium.org/developers/design-documents/linuxresourcesandlocalizedstrings
"""

from __future__ import print_function

import argparse
import hashlib
import os
import sys

# Import grit first to get local third_party modules.
import grit  # pylint: disable=ungrouped-imports,unused-import

import six

from grit.format import data_pack


def _RepackMain(args):
  data_pack.RePack(args.output_pak_file, args.input_pak_files, args.whitelist,
                   args.suppress_removed_key_output)


def _ExtractMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)

  for resource_id, payload in pak.resources.items():
    path = os.path.join(args.output_dir, str(resource_id))
    with open(path, 'w') as f:
      f.write(payload)


def _CreateMain(args):
  pak = {}
  for name in os.listdir(args.input_dir):
    try:
      resource_id = int(name)
    except:
      continue
    filename = os.path.join(args.input_dir, name)
    if os.path.isfile(filename):
      with open(filename, 'rb') as f:
        pak[resource_id] = f.read()
  data_pack.WriteDataPack(pak, args.output_pak_file, data_pack.UTF8)


def _PrintMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)
  output = args.output
  encoding = 'binary'
  if pak.encoding == 1:
    encoding = 'utf-8'
  elif pak.encoding == 2:
    encoding = 'utf-16'
  else:
    encoding = '?' + str(pak.encoding)

  output.write('version: {}\n'.format(pak.version))
  output.write('encoding: {}\n'.format(encoding))
  output.write('num_resources: {}\n'.format(len(pak.resources)))
  output.write('num_aliases: {}\n'.format(len(pak.aliases)))
  breakdown = ', '.join('{}: {}'.format(*x) for x in pak.sizes)
  output.write('total_size: {} ({})\n'.format(pak.sizes.total, breakdown))

  try_decode = args.decode and encoding.startswith('utf')
  # Print IDs in ascending order, since that's the order in which they appear in
  # the file (order is lost by Python dict).
  for resource_id in sorted(pak.resources):
    data = pak.resources[resource_id]
    canonical_id = pak.aliases.get(resource_id, resource_id)
    desc = '<data>'
    if try_decode:
      try:
        desc = six.text_type(data, encoding)
        if len(desc) > 60:
          desc = desc[:60] + u'...'
        desc = desc.replace('\n', '\\n')
      except UnicodeDecodeError:
        pass
    sha1 = hashlib.sha1(data).hexdigest()[:10]
    output.write(
        u'Entry(id={}, canonical_id={}, size={}, sha1={}): {}\n'.format(
            resource_id, canonical_id, len(data), sha1, desc).encode('utf-8'))


def _ListMain(args):
  pak = data_pack.ReadDataPack(args.pak_file)
  for resource_id in sorted(pak.resources):
    args.output.write('%d\n' % resource_id)


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)
  # Subparsers are required by default under Python 2.  Python 3 changed to
  # not required, but didn't include a required option until 3.7.  Setting
  # the required member works in all versions (and setting dest name).
  sub_parsers = parser.add_subparsers(dest='action')
  sub_parsers.required = True

  sub_parser = sub_parsers.add_parser('repack',
      help='Combines several .pak files into one.')
  sub_parser.add_argument('output_pak_file', help='File to create.')
  sub_parser.add_argument('input_pak_files', nargs='+',
      help='Input .pak files.')
  sub_parser.add_argument('--whitelist',
      help='Path to a whitelist used to filter output pak file resource IDs.')
  sub_parser.add_argument('--suppress-removed-key-output', action='store_true',
      help='Do not log which keys were removed by the whitelist.')
  sub_parser.set_defaults(func=_RepackMain)

  sub_parser = sub_parsers.add_parser('extract', help='Extracts pak file')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('-o', '--output-dir', default='.',
                          help='Directory to extract to.')
  sub_parser.set_defaults(func=_ExtractMain)

  sub_parser = sub_parsers.add_parser('create',
      help='Creates pak file from extracted directory.')
  sub_parser.add_argument('output_pak_file', help='File to create.')
  sub_parser.add_argument('-i', '--input-dir', default='.',
                          help='Directory to create from.')
  sub_parser.set_defaults(func=_CreateMain)

  sub_parser = sub_parsers.add_parser('print',
      help='Prints all pak IDs and contents. Useful for diffing.')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('--output', type=argparse.FileType('w'),
      default=sys.stdout,
      help='The resource list path to write (default stdout)')
  sub_parser.add_argument('--no-decode', dest='decode', action='store_false',
      default=True, help='Do not print entry data.')
  sub_parser.set_defaults(func=_PrintMain)

  sub_parser = sub_parsers.add_parser('list-id',
      help='Outputs all resource IDs to a file.')
  sub_parser.add_argument('pak_file')
  sub_parser.add_argument('--output', type=argparse.FileType('w'),
      default=sys.stdout,
      help='The resource list path to write (default stdout)')
  sub_parser.set_defaults(func=_ListMain)

  args = parser.parse_args()
  args.func(args)


if __name__ == '__main__':
  main()
