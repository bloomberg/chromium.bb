#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reads a trace. Mostly for testing."""

import logging
import optparse
import os
import sys

import trace_inputs

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(os.path.dirname(BASE_DIR))


def read_trace(logname, root_dir, cwd_dir, product_dir):
  # Resolve any symlink
  root_dir = os.path.realpath(root_dir)
  api = trace_inputs.get_api()
  _, _, _, _, simplified = trace_inputs.load_trace(logname, root_dir, api)
  variables = trace_inputs.generate_dict(simplified, cwd_dir, product_dir)
  trace_inputs.pretty_print(variables, sys.stdout)


def main():
  """CLI frontend to validate arguments."""
  parser = optparse.OptionParser(
      usage='%prog <options> [gtest]')
  parser.add_option(
      '-v', '--verbose',
      action='count',
      default=0,
      help='Use up to 3 times to increase logging level')
  parser.add_option(
      '-c', '--cwd',
      default='chrome',
      help='Signal to start the process from this relative directory. When '
           'specified, outputs the inputs files in a way compatible for '
           'gyp processing. Should be set to the relative path containing the '
           'gyp file, e.g. \'chrome\' or \'net\'')
  parser.add_option(
      '-p', '--product-dir',
      default='out/Release',
      help='Directory for PRODUCT_DIR. Default: %default')
  parser.add_option(
      '--root-dir',
      default=ROOT_DIR,
      help='Root directory to base everything off. Default: %default')
  options, args = parser.parse_args()

  level = [logging.ERROR, logging.INFO, logging.DEBUG][min(2, options.verbose)]
  logging.basicConfig(
        level=level,
        format='%(levelname)5s %(module)15s(%(lineno)3d):%(message)s')

  if len(args) != 1:
    parser.error('Please provide the log to read')
  if not options.product_dir:
    parser.error('--product-dir is required')
  if not options.cwd:
    parser.error('--cwd is required')

  return read_trace(
      args[0],
      options.root_dir,
      options.cwd,
      options.product_dir)


if __name__ == '__main__':
  sys.exit(main())
