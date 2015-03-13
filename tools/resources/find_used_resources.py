#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import sys

USAGE = """find_used_resources.py [-h] [-i INPUT] [-o OUTPUT]

Outputs the sorted list of resource ids that are part of unknown pragma warning
in the given build log.

This script is used to find the resources that are actually compiled in Chrome
in order to only include the needed strings/images in Chrome PAK files. The
script parses out the list of used resource ids. These resource ids show up in
the build output after building Chrome with gyp variable
enable_resource_whitelist_generation set to 1. This gyp flag causes the compiler
to print out a UnknownPragma message every time a resource id is used. E.g.:
foo.cc:22:0: warning: ignoring #pragma whitelisted_resource_12345
[-Wunknown-pragmas]

On Windows, the message is simply a message via __pragma(message(...)).

"""


def GetResourceIdsInPragmaWarnings(input):
  """Returns sorted set of resource ids that are inside unknown pragma warnings
     for the given input.
  """
  used_resources = set()
  unknown_pragma_warning_pattern = re.compile(
      'whitelisted_resource_(?P<resource_id>[0-9]+)')
  for ln in input:
    match = unknown_pragma_warning_pattern.search(ln)
    if match:
      resource_id = int(match.group('resource_id'))
      used_resources.add(resource_id)
  return sorted(used_resources)

def Main():
  parser = argparse.ArgumentParser(usage=USAGE)
  parser.add_argument(
      '-i', '--input', type=argparse.FileType('r'), default=sys.stdin,
      help='The build log to read (default stdin)')
  parser.add_argument(
      '-o', '--output', type=argparse.FileType('w'), default=sys.stdout,
      help='The resource list path to write (default stdout)')
  args = parser.parse_args()


  used_resources = GetResourceIdsInPragmaWarnings(args.input)
  for resource_id in used_resources:
    args.output.write('%d\n' % resource_id)

if __name__ == '__main__':
  Main()
