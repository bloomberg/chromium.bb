#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges multiple OS-specific gyp dependency lists into one that works on all
of them.

The logic is relatively simple. Takes the current conditions, add more
condition, find the strict subset. Done.
"""

import logging
import sys

from isolate import Configs, DEFAULT_OSES, eval_content, extract_comment
from isolate import load_isolate_as_config, union, convert_map_to_isolate_dict
from isolate import reduce_inputs, invert_map, print_all
import run_test_cases


def load_isolates(items, default_oses):
  """Parses each .isolate file and returns the merged results.

  It only loads what load_isolate_as_config() can process.

  Return values:
    files: dict(filename, set(OS where this filename is a dependency))
    dirs:  dict(dirame, set(OS where this dirname is a dependency))
    oses:  set(all the OSes referenced)
    """
  configs = Configs(default_oses, None)
  for item in items:
    logging.debug('loading %s' % item)
    with open(item, 'r') as f:
      content = f.read()
    new_config = load_isolate_as_config(
        eval_content(content), extract_comment(content), default_oses)
    logging.debug('has OSes: %s' % ','.join(k for k in new_config.per_os if k))
    configs = union(configs, new_config)
  logging.debug('Total OSes: %s' % ','.join(k for k in configs.per_os if k))
  return configs


def main(args=None):
  parser = run_test_cases.OptionParserWithLogging(
      usage='%prog <options> [file1] [file2] ...')
  parser.add_option(
      '-o', '--output', help='Output to file instead of stdout')
  parser.add_option(
      '--os', default=','.join(DEFAULT_OSES),
      help='Inject the list of OSes, default: %default')

  options, args = parser.parse_args(args)

  configs = load_isolates(args, options.os.split(','))
  data = convert_map_to_isolate_dict(
      *reduce_inputs(*invert_map(configs.flatten())))
  if options.output:
    with open(options.output, 'wb') as f:
      print_all(configs.file_comment, data, f)
  else:
    print_all(configs.file_comment, data, sys.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
