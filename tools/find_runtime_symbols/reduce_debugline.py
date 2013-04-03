#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Reduces result of 'readelf -wL' to just a set of address ranges per file.

For example:

CU: ../../chrome_main.cc:
File name                            Line number    Starting address
chrome_main.cc                                30            0xa3be90
(an empty line)
chrome_main.cc                                31            0xa3bea3
chrome_main.cc                                32            0xa3beaf
chrome_main.cc                                34            0xa3bec9
chrome_main.cc                                32            0xa3bed1
(an empty line)

The example above is reduced into:
{'../../chrome_main.cc', [(0xa3be90, 0xa3be90), (0xa3bea3, 0xa3bed1)]}
where (0xa3bea3, 0xa3bed1) means an address range from 0xa3bea3 to 0xa3bed1.

This script assumes that the result of 'readelf -wL' ends with an empty line.

Note: the option '-wL' has the same meaning with '--debug-dump=decodedline'.
"""

import re
import sys


_FILENAME_PATTERN = re.compile('(CU: |)(.+)\:')


def reduce_decoded_debugline(input_file):
  filename = ''
  ranges_dict = {}
  starting = None
  ending = None

  for line in input_file:
    line = line.strip()

    if line.endswith(':'):
      matched = _FILENAME_PATTERN.match(line)
      if matched:
        filename = matched.group(2)
      continue

    unpacked = line.split(None, 2)
    if len(unpacked) != 3 or not unpacked[2].startswith('0x'):
      if starting:
        ranges_dict.setdefault(filename, []).append((starting, ending))
        starting = None
        ending = None
      continue

    ending = int(unpacked[2], 16)
    if not starting:
      starting = ending

  if starting or ending:
    raise ValueError('No new line at last.')

  return ranges_dict


def main():
  if len(sys.argv) != 1:
    print >> sys.stderr, 'Unsupported arguments'
    return 1

  ranges_dict = reduce_decoded_debugline(sys.stdin)
  for filename, ranges in ranges_dict.iteritems():
    print filename + ':'
    prev = (0, 0)
    for address_range in sorted(ranges):
      if address_range == prev:
        continue
      print ' %x-%x' % (address_range[0], address_range[1])
      prev = address_range


if __name__ == '__main__':
  sys.exit(main())
