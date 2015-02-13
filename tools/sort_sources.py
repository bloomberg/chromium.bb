#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given a GYP/GN filename, sort C-ish source files in that file.

Shows a diff and prompts for confirmation before doing the deed.
Works great with tools/git/for-all-touched-files.py.
"""

import difflib
import optparse
import re
import sys

from yes_no import YesNo

SUFFIXES = ['c', 'cc', 'cpp', 'h', 'mm', 'rc', 'rc.version', 'ico', 'def',
            'release']
PATTERN = re.compile('^\s+[\'"].*\.(%s)[\'"],$' %
                     '|'.join([re.escape(x) for x in SUFFIXES]))

def SortSources(original_lines):
  """Sort source file names in |original_lines|.

  Args:
    original_lines: Lines of the original content as a list of strings.

  Returns:
    Lines of the sorted content as a list of strings.

  The algorithm is fairly naive. The code tries to find a list of C-ish source
  file names by a simple regex, then sort them. The code does not try to
  understand the syntax of the build files, hence there are many cases that
  the code cannot handle correctly (ex. comments within a list of source file
  names).
  """

  output_lines = []
  sources = []
  for line in original_lines:
    if re.search(PATTERN, line):
      sources.append(line)
    else:
      if sources:
        output_lines.extend(sorted(sources))
        sources = []
      output_lines.append(line)
  return output_lines


def ProcessFile(filename, should_confirm):
  """Process the input file and rewrite if needed.

  Args:
    filename: Path to the input file.
    should_confirm: If true, diff and confirmation prompt are shown.
  """

  original_lines = []
  with open(filename, 'r') as input_file:
    for line in input_file:
      original_lines.append(line)

  new_lines = SortSources(original_lines)
  if original_lines == new_lines:
    print '%s: no change' % filename
    return

  if should_confirm:
    diff = difflib.unified_diff(original_lines, new_lines)
    sys.stdout.writelines(diff)
    if not YesNo('Use new file (y/N)'):
      return

  with open(filename, 'w') as output_file:
    output_file.writelines(new_lines)


def main():
  parser = optparse.OptionParser(usage='%prog filename1 filename2 ...')
  parser.add_option('-f', '--force', action='store_false', default=True,
                    dest='should_confirm',
                    help='Turn off confirmation prompt.')
  opts, filenames = parser.parse_args()

  if len(filenames) < 1:
    parser.print_help()
    return 1

  for filename in filenames:
    ProcessFile(filename, opts.should_confirm)


if __name__ == '__main__':
  sys.exit(main())
