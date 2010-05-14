#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script concatenates in place JS files in the order specified
# using <script> tags in a given 'order.html' file.

from HTMLParser import HTMLParser
from cStringIO import StringIO
import os.path
import sys

class OrderedJSFilesExtractor(HTMLParser):

  def __init__(self, order_html_name):
    HTMLParser.__init__(self)
    self.ordered_js_files = []
    order_html = open(order_html_name, 'r')
    self.feed(order_html.read())

  def handle_starttag(self, tag, attrs):
    if tag == 'script':
      attrs_dict = dict(attrs)
      if ('type' in attrs_dict and attrs_dict['type'] == 'text/javascript' and
          'src' in attrs_dict):
        self.ordered_js_files.append(attrs_dict['src'])

class PathExpander:

  def __init__(self, paths):
    self.paths = paths;

  def expand(self, filename):
    last_path = None
    expanded_name = None
    for path in self.paths:
      fname = "%s/%s" % (path, filename)
      if (os.access(fname, os.F_OK)):
        if (last_path != None):
          raise Exception('Ambiguous file %s: found in %s and %s' %
                          (filename, last_path, path))
        expanded_name = fname
        last_path = path
    return expanded_name

def main(argv):

  if len(argv) < 3:
    print('usage: %s order.html input_source_dir_1 input_source_dir_2 ... '
          'output_file' % argv[0])
    return 1

  output_file_name = argv.pop()
  extractor = OrderedJSFilesExtractor(argv[1])
  expander = PathExpander(argv[2:])
  output = StringIO()

  for input_file_name in extractor.ordered_js_files:
    full_path = expander.expand(input_file_name)
    if (full_path is None):
      raise Exception('File %s referenced in %s not found on any source paths, '
                      'check source tree for consistency' %
                      (input_file_name, input_file_name))
    output.write('/* %s */\n\n' % input_file_name)
    input_file = open(full_path, 'r')
    output.write(input_file.read())
    output.write('\n')
    input_file.close()

  output_file = open(output_file_name, 'w')
  output_file.write(output.getvalue())
  output_file.close()
  output.close()

  # Touch output file directory to make sure that Xcode will copy
  # modified resource files.
  if sys.platform == 'darwin':
    output_dir_name = os.path.dirname(output_file_name)
    os.utime(output_dir_name, None)

if __name__ == '__main__':
  sys.exit(main(sys.argv))
