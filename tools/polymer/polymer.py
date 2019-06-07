# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Helper script for inlining HTML content from an HTML to a JS file. This is
# necessary for Polymer3 UI elements. The following |html_type| options are
# provided
# - dom-module: Extracts HTML content from a Polymer2 HTML file hosting a
#               dom-module.
# - custom-style: Extracts HTML content from a Polymer HTML file hosting a
#                 custom-style.
# - v3-ready: Extracts HTML content from a file that is only used in Polymer3.
#
# "dom-module" and "custom-style" are useful for avoiding duplicating HTML code
# between Polymer2 and Polymer3 while migration is in progress.
#
# Note: Having multiple <dom-module>s within a single HTML file is not currently
# supported by this script.

import argparse
import os
import re
import sys

_CWD = os.getcwd()

HTML_TEMPLATE_REGEX = '{__html_template__}'


def ExtractTemplate(html_file, html_type):
  if html_type == 'v3-ready':
    with open(html_file, 'r') as f:
      return f.read()

  if html_type == 'dom-module':
    with open(html_file, 'r') as f:
      lines = f.readlines()
      start_line = -1
      end_line = -1
      for i, line in enumerate(lines):
        if re.match(r'\s*<dom-module ', line):
          assert start_line == -1
          assert end_line == -1
          assert re.match(r'\s*<template', lines[i + 1])
          start_line = i + 2;
        if re.match(r'\s*</dom-module>', line):
          assert start_line != -1
          assert end_line == -1
          assert re.match(r'\s*</template>', lines[i - 2])
          assert re.match(r'\s*<script ', lines[i - 1])
          end_line = i - 3;
    return ''.join(lines[start_line:end_line + 1])

  assert html_type == 'custom-style'
  with open(html_file, 'r') as f:
    lines = f.readlines()
    start_line = -1
    end_line = -1
    for i, line in enumerate(lines):
      if re.match(r'\s*<custom-style>', line):
        assert start_line == -1
        assert end_line == -1
        start_line = i;
      if re.match(r'\s*</custom-style>', line):
        assert start_line != -1
        assert end_line == -1
        end_line = i;

  return ''.join(lines[start_line:end_line + 1])


def ProcessFile(js_file, html_file, html_type, out_folder):
  html_template = ExtractTemplate(html_file, html_type)

  with open(js_file) as f:
    lines = f.readlines()

  for i, line in enumerate(lines):
    line = line.replace(HTML_TEMPLATE_REGEX, html_template)
    lines[i] = line

  # Reconstruct file.
  out_filename = os.path.basename(js_file)

  with open(os.path.join(out_folder, out_filename), 'w') as f:
    for l in lines:
      f.write(l)
  return

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  parser.add_argument('--js_file', required=True)
  parser.add_argument('--html_file', required=True)
  parser.add_argument(
      '--html_type', choices=['dom-module', 'custom-style', 'v3-ready'],
      required=True)
  args = parser.parse_args(argv)

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  ProcessFile(
      os.path.join(in_folder, args.js_file),
      os.path.join(in_folder, args.html_file),
      args.html_type, out_folder)


if __name__ == '__main__':
  main(sys.argv[1:])
