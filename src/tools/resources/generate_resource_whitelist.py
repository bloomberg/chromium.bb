#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

__doc__ = """generate_resource_whitelist.py [-o OUTPUT] INPUT

INPUT is the path to an unstripped binary containing references to
resources in its debug info.

This script generates a resource whitelist by reading debug info from
INPUT and writes it to OUTPUT.
"""

import argparse
import subprocess
import sys


def WriteResourceWhitelist(args):
  # Produce a resource whitelist by searching for debug info referring to
  # instantiations of the special function ui::WhitelistedResource (see
  # ui/base/resource/whitelist.h). It's sufficient to look for strings in
  # .debug_str rather than trying to parse all of the debug info.
  readelf = subprocess.Popen(
      ['readelf', '-p', '.debug_str', args.input], stdout=subprocess.PIPE)
  resource_ids = set()
  output_exists = False
  for line in readelf.stdout:
    output_exists = True
    # Read a line of the form "  [   123]  WhitelistedResource<456>". We're
    # only interested in the string, not the offset. We're also not interested
    # in header lines.
    split = line.split(']', 1)
    if len(split) < 2:
      continue
    s = split[1][2:]
    if s.startswith('WhitelistedResource<'):
      try:
        resource_ids.add(int(s[len('WhitelistedResource<'):-len('>')-1]))
      except ValueError:
        continue
  if not output_exists:
    raise Exception('No debug info was dumpped. Ensure GN arg "symbol_level" '
                    '!= 0 and that the file is not stripped.')

  for id in sorted(resource_ids):
    args.output.write(str(id) + '\n')
  exit_code = readelf.wait()
  if exit_code != 0:
    raise Exception('readelf exited with exit code %d' % exit_code)


def main():
  parser = argparse.ArgumentParser(usage=__doc__)
  parser.add_argument('input', help='An unstripped binary.')
  parser.add_argument(
      '-o', dest='output', type=argparse.FileType('w'), default=sys.stdout,
      help='The resource list path to write (default stdout)')

  args = parser.parse_args()
  WriteResourceWhitelist(args)

if __name__ == '__main__':
  main()
