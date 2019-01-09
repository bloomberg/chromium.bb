# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


_VAR_HEX_REG = r'(\s*--)([a-zA-Z0-9_-]+\s*)(:\s*)#([a-fA-F0-9]+)(\s*;\s*)'


def Rgbify(content, prefix=''):
  lines = content.splitlines()
  rgb_version = []

  for line in lines:
    rgb_version.append(line)

    match = re.match(_VAR_HEX_REG, line)
    if not match:
      continue

    if prefix and not match.group(2).startswith(prefix):
      continue

    before, name, during, hex, after = match.groups()
    r, g, b = int(hex[0:2], 16), int(hex[2:4], 16), int(hex[4:6], 16)
    rgb = '%d, %d, %d' % (r, g, b)
    rgb_version.append(''.join([before, '%s-rgb' % name, during, rgb, after]))

  return '\n'.join(rgb_version)


def RgbifyFileInPlace(path, prefix):
  rgbified = Rgbify(open(path, 'r').read(), prefix)
  with open(path, 'w') as path_file:
    path_file.write(rgbified)


if __name__ == '__main__':
  import argparse
  import sys
  parser = argparse.ArgumentParser('Add an -rgb equivalent to --var: #hex;')
  parser.add_argument('--filter-prefix', type=str, default='')
  parser.add_argument('paths', nargs='+', help='File path to add -rgb vars to')
  opts = parser.parse_args(sys.argv[1:])

  for path in opts.paths:
    RgbifyFileInPlace(path, opts.filter_prefix)
