#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from HTMLParser import HTMLParser
import subprocess
import sys
import os

DESCRIPTION = '''Extract the <script> tags from the specified html file and
run them through jscompile, along with any additional JS files specified.'''
FILES_HELP = '''A list of HTML or Javascript files. The Javascript files should
contain definitions of types or functions that are known to Chrome but not to
jscompile; they are added to the list of <script> tags found in each of the
HTML files.'''
STAMP_HELP = 'Timestamp file to update on success.'

class ScriptTagParser(HTMLParser):
  def __init__(self):
    HTMLParser.__init__(self)
    self.scripts = []

  def handle_starttag(self, tag, attrs):
    if tag == 'script':
      for (name, value) in attrs:
        if name == 'src':
          self.scripts.append(value)


def parseHtml(html_file, js_proto_files):
  src_dir = os.path.split(html_file)[0]
  f = open(html_file, 'r')
  html = f.read()
  parser = ScriptTagParser();
  parser.feed(html)
  scripts = []
  for script in parser.scripts:
    # Ignore non-local scripts
    if not '://' in script:
      scripts.append(os.path.join(src_dir, script))

  args = ['jscompile'] + scripts + js_proto_files
  result = subprocess.call(args)
  return result == 0


def main():
  parser = argparse.ArgumentParser(description = DESCRIPTION)
  parser.add_argument('files', nargs = '+', help = FILES_HELP)
  parser.add_argument('--success-stamp', dest = 'success_stamp',
                      help = STAMP_HELP)
  options = parser.parse_args()

  html = []
  js = []
  for file in options.files:
    name, extension = os.path.splitext(file)
    if extension == '.html':
      html.append(file)
    elif extension == '.js':
      js.append(file)
    else:
      print >> sys.stderr, 'Unknown extension (' + extension + ') for ' + file
      return 1

  for html_file in html:
    if not parseHtml(html_file, js):
      return 1

  if options.success_stamp:
    with open(options.success_stamp, 'w'):
      os.utime(options.success_stamp, None)

  return 0

if __name__ == '__main__':
  sys.exit(main())
