# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

# A tool to add "#pragma once" lines to files that don't have it yet.
# Intended usage:
#   find chrome -name '*.h' -exec  python tools/pragmaonce/pragmaonce.py {} \;

# Some files have absurdly long comments at the top
NUM_LINES_TO_SCAN_FOR_GUARD = 250

def main(filename):
  f = open(filename)
  lines = f.readlines()
  f.close()

  index = -1
  for i in xrange(min(NUM_LINES_TO_SCAN_FOR_GUARD, len(lines) - 1)):
    m1 = re.match(r'^#ifndef ([A-Z_]+)', lines[i])
    m2 = re.match(r'^#define ([A-Z_]+)', lines[i + 1])
    if m1 and m2:
      if m1.group(1) != m2.group(1):
        print 'Skipping', filename, \
              ': Broken include guard (%s, %s)' % (m1.group(1), m2.group(1))
      index = i + 2
      break

  if index == -1:
    print 'Skipping', filename, ': no include guard found'
    return

  if index < len(lines) and re.match(r'#pragma once', lines[index]):
    # The pragma is already there.
    return

  lines.insert(index, "#pragma once\n")

  f = open(filename, 'w')
  f.write(''.join(lines))
  f.close()

if __name__ == '__main__':
  if len(sys.argv) != 2:
    print >>sys.stderr, "Usage: %s inputfile" % sys.argv[0]
    sys.exit(1)
  main(sys.argv[1])
