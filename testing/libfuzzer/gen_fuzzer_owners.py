#!/usr/bin/python2
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate owners (.owners file) by looking at commit author for
libfuzzer test.

Invoked by GN from fuzzer_test.gni.
"""

import argparse
import os
import subprocess
import sys


def GetOwnerForFuzzer(sources):
  """Return owner given a list of sources as input."""
  for source in sources:
    if not os.path.exists(source):
      continue
    with open(source, 'r') as source_file_handle:
      source_content = source_file_handle.read()
    if ('PROTO_FUZZER' in source_content or
        'LLVMFuzzerTestOneInput' in source_content):
      # Found the fuzzer source (and not dependency of fuzzer).
      return subprocess.check_output(
          ['git', 'log', '--reverse', '--format=%ae', '-1', source])

  return None


def main():
  parser = argparse.ArgumentParser(description='Generate fuzzer owners.')
  parser.add_argument('--owners', required=True)
  parser.add_argument('--sources', nargs='+')
  args = parser.parse_args()

  owner = GetOwnerForFuzzer(args.sources)

  # Generate owners file.
  with open(args.owners, 'w') as owners_file:
    # If we found an owner, then write it to file.
    # Otherwise, leave empty file to keep ninja happy.
    if owner:
      owners_file.write(owner)


if __name__ == '__main__':
  main()
