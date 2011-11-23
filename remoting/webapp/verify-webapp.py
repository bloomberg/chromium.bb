#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Verifies that a given messages.json file defines all the strings used by the
a given set of files. For file formats where it is not possible to infer which
strings represent message identifiers, localized strings should be explicitly
annotated with the string "i18n-content", for example:

  LocalizeString(/*i18n-content*/"PRODUCT_NAME");

This script also recognises localized strings in HTML and manifest.json files:

  HTML:          <span i18n-content="PRODUCT_NAME"></span>
  manifest.json: __MSG_PRODUCT_NAME__

Note that these forms must be exact; extra spaces are not permitted, though
either single or double quotes are recognized.

In addition, the script checks that all the messages are still in use; if
this is not the case then a warning is issued, but the script still succeeds.
"""

import json
import os
import re
import sys

all_tags = set([])

WARNING_MESSAGE = """
To remove this warning, either remove the unused tags from
messages.json, add the files that use the tags listed above to
remoting.gyp, or annotate existing uses of those tags with the
prefix /*i18n-content*/
"""


def ExtractTagFromLine(line):
  """Extract a tag from a line of HTML, C++, JS or JSON."""
  # HTML-style
  m = re.search('i18n-content=[\'"]([^\'"]*)[\'"]', line)
  if m: return m.group(1)
  # C++/Javascript style
  m = re.search('/\*i18n-content\*/[\'"]([^\`"]*)[\'"]', line)
  if m: return m.group(1)
  # Manifest style
  m = re.search('__MSG_(.*)__', line)
  if m: return m.group(1)
  return None


def CheckFileForUnknownTag(filename, messages):
  """Parse |filename|, looking for tags and report any that are not included in
     |messages|. Return True if all tags are present and correct, or False if
     any are missing. If no tags are found, print a warning message and return
     True."""
  result = True
  matches = False
  f = open(filename, 'r')
  lines = f.readlines()
  for i in xrange(0, len(lines)):
    tag = ExtractTagFromLine(lines[i])
    if tag:
      all_tags.add(tag)
      matches = True
      if not tag in messages:
        result = False
        print '%s/%s:%d: error: Undefined tag: %s' % \
            (os.getcwd(), filename, i + 1, tag)
  if not matches:
    print '%s/%s:0: warning: No tags found' % (os.getcwd(), filename)
  f.close()
  return result


def main():
  if len(sys.argv) < 4:
    print 'Usage: verify-webapp.py <touch> <messages> <message_users...>'
    return 1

  touch_file = sys.argv[1]
  messages = json.load(open(sys.argv[2], 'r'))
  exit_code = 0
  for f in sys.argv[3:]:
    if not CheckFileForUnknownTag(f, messages):
      exit_code = 1

  warnings = False
  for tag in messages:
    if tag not in all_tags:
      print ('%s/%s:0: warning: %s is defined but not used') % \
          (os.getcwd(), sys.argv[2], tag)
      warnings = True
  if warnings:
    print WARNING_MESSAGE

  if exit_code == 0:
    f = open(touch_file, 'a')
    f.close()
    os.utime(touch_file, None)

  return exit_code


if __name__ == '__main__':
  sys.exit(main())
