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

def main(argv):

  if len(argv) < 3:
    print('usage: %s order.html input_file_1 input_file_2 ... '
          'output_file' % argv[0])
    return 1

  output_file_name = argv.pop()

  full_paths = {}
  for full_path in argv[2:]:
    file_name = os.path.basename(full_path)
    if file_name.endswith('.js'):
      full_paths[file_name] = full_path

  extractor = OrderedJSFilesExtractor(argv[1])
  output = StringIO()

  for input_file_name in extractor.ordered_js_files:
    if not input_file_name in full_paths:
      print('A full path to %s isn\'t specified in command line. '
            'Probably, you have an obsolete script entry in %s' %
            (input_file_name, argv[1]))
    output.write('/* %s */\n\n' % input_file_name)
    input_file = open(full_paths[input_file_name], 'r')
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
