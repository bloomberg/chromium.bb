#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolbox to manage all the json files in this directory.

It can reformat them in their canonical format or ensures they are well
formatted.
"""

import argparse
import glob
import json
import os
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))


# These are not 'builders'.
SKIP = {
  'compile_targets', 'gtest_tests', 'filter_compile_builders',
  'non_filter_builders', 'non_filter_tests_builders',
}


def upgrade_test(test):
  """Converts from old style string to new style dict."""
  if isinstance(test, basestring):
    return {'test': test}
  assert isinstance(test, dict)
  return test


def main():
  parser = argparse.ArgumentParser(description=sys.modules[__name__].__doc__)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-c', '--check', action='store_true', help='Only check the files')
  group.add_argument(
      '-w', '--write', action='store_true', help='Rewrite the files')
  args = parser.parse_args()

  result = 0
  for filepath in glob.glob(os.path.join(THIS_DIR, '*.json')):
    filename = os.path.basename(filepath)
    with open(filepath) as f:
      content = f.read()
    config = json.loads(content)
    for builder, data in sorted(config.iteritems()):
      if builder in SKIP:
        # Oddities.
        continue

      if not isinstance(data, dict):
        print('%s: %s is broken: %s' % (filename, builder, data))
        continue

      if 'gtest_tests' in data:
        config[builder]['gtest_tests'] = sorted(
          (upgrade_test(l) for l in data['gtest_tests']),
          key=lambda x: x['test'])

    expected = json.dumps(
        config, sort_keys=True, indent=2, separators=(',', ': ')) + '\n'
    if content != expected:
      result = 1
      if args.write:
        with open(filepath, 'wb') as f:
          f.write(expected)
        print('Updated %s' % filename)
      else:
        print('%s is not in canonical format' % filename)
  return result


if __name__ == "__main__":
  sys.exit(main())
