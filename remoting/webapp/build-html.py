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
  def __init__(self, template_files, js_files):
    self.js_files = js_files

    self.templates_expected = set()
    for template in template_files:
      self.templates_expected.add(os.path.basename(template))

    self.templates_found = set()

  def includeJavascript(self, output):
    for js_path in sorted(self.js_files):
      js_file = os.path.basename(js_path)
      output.write('    <script src="' + js_file + '"></script>\n')

  def verifyTemplateList(self):
    """Verify that all the expected templates were found."""
    if self.templates_expected > self.templates_found:
      extra = self.templates_expected - self.templates_found
      print 'Extra templates specified:', extra
      return False
    return True

  def validateTemplate(self, template_path):
    template = os.path.basename(template_path)
    if template in self.templates_expected:
      self.templates_found.add(template)
      return True
    return False

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
          prefix = m.group(1)
          template_name = m.group(2)
          if not self.validateTemplate(template_name):
            error('Found template not in list of expected templates: %s' %
                  template_name)
          self.processTemplate(output, template_name, indent + len(prefix))
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
  print ('Usage: %s <output-file> <input-template> '
         '[--templates <template-files...>] '
         '[--js <js-files...>]' % sys.argv[0])
  print 'where:'
  print '  <output-file> Path to HTML output file'
  print '  <input-template> Path to input template'
  print '  <template-files> The html template files used by <input-template>'
  print '  <js-files> The Javascript files to include in HTML <head>'


def main():
  if len(sys.argv) < 4:
    show_usage()
    error('Not enough arguments')

  out_file = sys.argv[1]
  main_template_file = sys.argv[2]

  template_files = []
  js_files = []
  for arg in sys.argv[3:]:
    if arg == '--js' or arg == '--template':
      arg_type = arg
    elif arg_type == '--js':
      js_files.append(arg)
    elif arg_type == '--template':
      template_files.append(arg)
    else:
      error('Unrecognized argument: %s' % arg)

  # Generate the main HTML file from the templates.
  with open(out_file, 'w') as output:
    gen = GenerateWebappHtml(template_files, js_files)
    gen.processTemplate(output, main_template_file, 0)

    # Verify that all the expected templates were found.
    if not gen.verifyTemplateList():
      error('Extra templates specified')

  # Verify that the generated HTML file is valid.
  with open(out_file, 'r') as input:
    parser = HtmlChecker()
    parser.feed(input.read())


if __name__ == '__main__':
  sys.exit(main())
