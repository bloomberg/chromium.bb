#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Builds the complete main.html file from the basic components.
"""

from HTMLParser import HTMLParser
import os
import re
import sys


def error(msg):
  print 'Error: %s' % msg
  sys.exit(1)


class HtmlChecker(HTMLParser):
  def __init__(self):
    HTMLParser.__init__(self)
    self.ids = set()

  def handle_starttag(self, tag, attrs):
    for (name, value) in attrs:
      if name == 'id':
        if value in self.ids:
          error('Duplicate id: %s' % value)
        self.ids.add(value)


class GenerateWebappHtml:
  def __init__(self, js_files):
    self.js_files = js_files

  def includeJavascript(self, output):
    for js_path in sorted(self.js_files):
      js_file = os.path.basename(js_path)
      output.write('    <script src="' + js_file + '"></script>\n')

  def processTemplate(self, output, template_file, indent):
    with open(template_file, 'r') as input:
      first_line = True
      skip_header_comment = False

      for line in input:
        # If the first line is the start of a copyright notice, then
        # skip over the entire comment.
        # This will remove the copyright info from the included files,
        # but leave the one on the main template.
        if first_line and re.match(r'<!--', line):
          skip_header_comment = True
        first_line = False
        if skip_header_comment:
          if re.search(r'-->', line):
            skip_header_comment = False
          continue

        m = re.match(
            r'^(\s*)<meta-include src="(.+)"\s*/>\s*$',
            line)
        if m:
          self.processTemplate(output, m.group(2), indent + len(m.group(1)))
          continue

        m = re.match(r'^\s*<meta-include type="javascript"\s*/>\s*$', line)
        if m:
          self.includeJavascript(output)
          continue

        if line.strip() == '':
          output.write('\n')
        else:
          output.write((' ' * indent) + line)


def show_usage():
  print 'Usage: %s <output-file> <input-template> <js-file>+' % sys.argv[0]
  print '  <output-file> Path to HTML output file'
  print '  <input-template> Path to input template'
  print '  <js-file> One or more Javascript files to include in HTML output'


def main():
  if len(sys.argv) < 4:
    show_usage()
    error('Not enough arguments')

  out_file = sys.argv[1]
  main_template_file = sys.argv[2]
  js_files = sys.argv[3:]

  # Generate the main HTML file from the templates.
  with open(out_file, 'w') as output:
    gen = GenerateWebappHtml(js_files)
    gen.processTemplate(output, main_template_file, 0)

  # Verify that the generated HTML file is valid.
  with open(out_file, 'r') as input:
    parser = HtmlChecker()
    parser.feed(input.read())


if __name__ == '__main__':
  sys.exit(main())
