#!/usr/bin/env python2.7
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
import re
import subprocess
import sys

AUTHOR_REGEX = re.compile('author-mail <(.+)>')
THIRD_PARTY_SEARCH_STRING = 'third_party' + os.sep
OWNERS_FILENAME = 'OWNERS'


def GetAuthorFromGitBlame(blame_output):
  """Return author from git blame output."""
  for line in blame_output.splitlines():
    m = AUTHOR_REGEX.match(line)
    if m:
      return m.group(1)

  return None


def GetOwnersIfThirdParty(source):
  """Return owners using OWNERS file if in third_party."""
  match_index = source.find(THIRD_PARTY_SEARCH_STRING)
  if match_index == -1:
    # Not in third_party, skip.
    return None

  match_index_with_library = source.find(
      os.sep, match_index + len(THIRD_PARTY_SEARCH_STRING))
  if match_index_with_library == -1:
    # Unable to determine library name, skip.
    return None

  owners_file_path = os.path.join(source[:match_index_with_library],
                                  OWNERS_FILENAME)
  if not os.path.exists(owners_file_path):
    return None

  return open(owners_file_path).read()


def GetOwnersForFuzzer(sources):
  """Return owners given a list of sources as input."""
  for source in sources:
    if not os.path.exists(source):
      continue

    with open(source, 'r') as source_file_handle:
      source_content = source_file_handle.read()

    if ('LLVMFuzzerTestOneInput' in source_content or
        'PROTO_FUZZER' in source_content):
      # Found the fuzzer source (and not dependency of fuzzer).

      is_git_file = bool(subprocess.check_output(['git', 'ls-files', source]))
      if not is_git_file:
        # File is not in working tree. Return owners for third_party.
        return GetOwnersIfThirdParty(source)

      # git log --follow and --reverse don't work together and using just
      # --follow is too slow. Make a best estimate with an assumption that
      # the original author has authored line 1 which is usually the
      # copyright line and does not change even with file rename / move.
      blame_output = subprocess.check_output(
          ['git', 'blame', '--porcelain', '-L1,1', source])
      return GetAuthorFromGitBlame(blame_output)

  return None


def main():
  parser = argparse.ArgumentParser(description='Generate fuzzer owners file.')
  parser.add_argument('--owners', required=True)
  parser.add_argument('--sources', nargs='+')
  args = parser.parse_args()

  owners = GetOwnersForFuzzer(args.sources)

  # Generate owners file.
  with open(args.owners, 'w') as owners_file:
    # If we found an owner, then write it to file.
    # Otherwise, leave empty file to keep ninja happy.
    if owners:
      owners_file.write(owners)


if __name__ == '__main__':
  main()
