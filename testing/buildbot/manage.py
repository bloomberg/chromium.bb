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
import subprocess
import sys


THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(THIS_DIR))
sys.path.insert(0, os.path.join(SRC_DIR, 'third_party', 'colorama', 'src'))

import colorama


SKIP = {
  # These are not 'builders'.
  'compile_targets', 'gtest_tests', 'filter_compile_builders',
  'non_filter_builders', 'non_filter_tests_builders',

  # These are not supported on Swarming yet.
  # http://crbug.com/472205
  'Chromium Mac 10.10',
  # http://crbug.com/441429
  'Linux Trusty (32)', 'Linux Trusty (dbg)(32)',

  # http://crbug.com/480053
  'Linux GN',
  'linux_chromium_gn_rel',

  # Unmaintained builders on chromium.fyi
  'ClangToTMac',
  'ClangToTMacASan',

  # One off builders. Note that Swarming does support ARM.
  'Linux ARM Cross-Compile',
  'Site Isolation Linux',
  'Site Isolation Win',
}


def upgrade_test(test):
  """Converts from old style string to new style dict."""
  if isinstance(test, basestring):
    return {'test': test}
  assert isinstance(test, dict)
  return test


def get_isolates():
  """Returns the list of all isolate files."""
  files = subprocess.check_output(['git', 'ls-files'], cwd=SRC_DIR).splitlines()
  return [os.path.basename(f) for f in files if f.endswith('.isolate')]


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

  if args.convert or args.remaining:
    isolates = get_isolates()

  if args.convert:
    if not args.test_name:
      parser.error('A test name is required with --convert')
    if args.test_name + '.isolate' not in isolates:
      parser.error('Create %s.isolate first' % args.test_name)

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
    try:
      config = json.loads(content)
    except ValueError as e:
      print "Exception raised while checking %s: %s" % (filepath, e)
      raise
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
            test.setdefault('swarming', {})
            if not test['swarming'].get('can_use_on_swarming_builders'):
              print('- %s: %s' % (filename, builder))
              test['swarming']['can_use_on_swarming_builders'] = True

    expected = json.dumps(
        config, sort_keys=True, indent=2, separators=(',', ': ')) + '\n'
    if content != expected:
      result = 1
      if args.write or args.convert:
        with open(filepath, 'wb') as f:
          f.write(expected)
        if args.write:
          print('Updated %s' % filename)
      else:
        print('%s is not in canonical format' % filename)
        print('run `testing/buildbot/manage.py -w` to fix')

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
      print('%-*s%sLocal       %sSwarming  %sMissing isolate' %
          (l, 'Test', colorama.Fore.RED, colorama.Fore.GREEN,
            colorama.Fore.MAGENTA))
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
        missing_isolate = ''
        if name + '.isolate' not in isolates:
          missing_isolate = colorama.Fore.MAGENTA + '*'
        print('%s%-*s %4d           %4d    %s' %
            (c, l, name, location['count_run_local'],
              location['count_run_on_swarming'], missing_isolate))

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
