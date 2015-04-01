#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolbox to manage all the json files in this directory.

It can reformat them in their canonical format or ensures they are well
formatted.
"""

import argparse
import collections
import glob
import json
import os
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(
    0, os.path.join(THIS_DIR, '..', '..', 'third_party', 'colorama', 'src'))

import colorama

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
  colorama.init()
  parser = argparse.ArgumentParser(description=sys.modules[__name__].__doc__)
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-c', '--check', action='store_true', help='Only check the files')
  group.add_argument(
      '--convert', action='store_true',
      help='Convert a test to run on Swarming everywhere')
  group.add_argument(
      '--remaining', action='store_true',
      help='Count the number of tests not yet running on Swarming')
  group.add_argument(
      '-w', '--write', action='store_true', help='Rewrite the files')
  parser.add_argument(
      'test_name', nargs='?',
      help='The test name to print which configs to update; only to be used '
           'with --remaining')
  args = parser.parse_args()

  if args.convert and not args.test_name:
    parser.error('A test name is required with --convert')

  # Stats when running in --remaining mode;
  tests_location = collections.defaultdict(
      lambda: {
        'count_run_local': 0, 'count_run_on_swarming': 0, 'local_configs': {}
      })

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

        if args.remaining:
          for test in data['gtest_tests']:
            name = test['test']
            if test.get('swarming', {}).get('can_use_on_swarming_builders'):
              tests_location[name]['count_run_on_swarming'] += 1
            else:
              tests_location[name]['count_run_local'] += 1
              tests_location[name]['local_configs'].setdefault(
                  filename, []).append(builder)
        elif args.convert:
          for test in data['gtest_tests']:
            if test['test'] != args.test_name:
              continue
            test.setdefault('swarming', {})['can_use_on_swarming_builders'] = (
                True)

    expected = json.dumps(
        config, sort_keys=True, indent=2, separators=(',', ': ')) + '\n'
    if content != expected:
      result = 1
      if args.write or args.convert:
        with open(filepath, 'wb') as f:
          f.write(expected)
        print('Updated %s' % filename)
      else:
        print('%s is not in canonical format' % filename)

  if args.remaining:
    if args.test_name:
      if args.test_name not in tests_location:
        print('Unknown test %s' % args.test_name)
        return 1
      for config, builders in sorted(
          tests_location[args.test_name]['local_configs'].iteritems()):
        print('%s:' % config)
        for builder in sorted(builders):
          print('  %s' % builder)
    else:
      l = max(map(len, tests_location))
      print('%-*s%sLocal       %sSwarming' %
          (l, 'Test', colorama.Fore.RED, colorama.Fore.GREEN))
      total_local = 0
      total_swarming = 0
      for name, location in sorted(tests_location.iteritems()):
        if not location['count_run_on_swarming']:
          c = colorama.Fore.RED
        elif location['count_run_local']:
          c = colorama.Fore.YELLOW
        else:
          c = colorama.Fore.GREEN
        total_local += location['count_run_local']
        total_swarming += location['count_run_on_swarming']
        print('%s%-*s %4d           %4d' %
            (c, l, name, location['count_run_local'],
              location['count_run_on_swarming']))
      total = total_local + total_swarming
      p_local = 100. * total_local / total
      p_swarming = 100. * total_swarming / total
      print('%s%-*s %4d (%4.1f%%)   %4d (%4.1f%%)' %
          (colorama.Fore.WHITE, l, 'Total:', total_local, p_local,
            total_swarming, p_swarming))
      print('%-*s                %4d' % (l, 'Total executions:', total))
  return result


if __name__ == "__main__":
  sys.exit(main())
