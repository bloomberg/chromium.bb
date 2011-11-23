#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that the message tags in the 2nd and subsequent JSON message files
match those specified in the first.

This is typically run when the translations are updated before a release, to
check that nothing got missed.
"""

import json
import sys
import string

def CheckTranslation(filename, translation, messages):
  """Check |messages| for missing Ids in |translation|, and similarly for unused
     Ids.  If there are missing/unused Ids then report them and return failure.
  """
  message_tags = set(map(string.lower, messages.keys()))
  translation_tags = set(map(string.lower, translation.keys()))
  if message_tags == translation_tags:
    return True

  undefined_tags = message_tags - translation_tags
  if undefined_tags:
    print '%s: Missing: %s' % (filename, ", ".join(undefined_tags))

  unused_tags = translation_tags - message_tags
  if unused_tags:
    print '%s: Unused: %s' % (filename, ", ".join(unused_tags))

  return False


def main():
  if len(sys.argv) < 3:
    print 'Usage: verify-translations.py <messages> <translation-files...>'
    return 1

  en_messages = json.load(open(sys.argv[1], 'r'))
  exit_code = 0
  for f in sys.argv[2:]:
    translation = json.load(open(f, 'r'))
    if not CheckTranslation(f, translation, en_messages):
      exit_code = 1

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
