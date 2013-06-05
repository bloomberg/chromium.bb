#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import optparse
import sys

import spec
import spec_val
import test_format


def AssertEquals(actual, expected):
  if actual != expected:
    raise AssertionError('\nEXPECTED:\n"""\n%s"""\n\nACTUAL:\n"""\n%s"""'
                         % (expected, actual))


def GetSpecValOutput(options, hex_content):
    validator_cls = {32: spec_val.Validator32}[options.bits]

    data = ''.join(test_format.ParseHex(hex_content))
    data += '\x90' * (-len(data) % spec.BUNDLE_SIZE)

    val = validator_cls(data)
    val.Validate()

    if val.messages == []:
      return 'SAFE\n'

    return ''.join(
        '%x: %s\n' % (offset, message) for offset, message in val.messages)


def Test(options, items_list):
  info = dict(items_list)

  if 'spec' in info:
    content = GetSpecValOutput(options, info['hex'])

    print '  Checking spec field...'
    if options.update:
      if content != info['spec']:
        print '  Updating spec field...'
        info['spec'] = content
    else:
      AssertEquals(content, info['spec'])

  # Update field values, but preserve their order.
  items_list = [(field, info[field]) for field, _ in items_list]

  return items_list


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--bits',
                    type=int,
                    help='The subarchitecture to run tests against: 32 or 64')
  parser.add_option('--update',
                    default=False,
                    action='store_true',
                    help='Regenerate golden fields instead of testing')

  options, args = parser.parse_args(args)

  if options.bits not in [32, 64]:
    parser.error('specify --bits 32 or --bits 64')

  if len(args) == 0:
    parser.error('No test files specified')
  processed = 0
  for glob_expr in args:
    test_files = sorted(glob.glob(glob_expr))
    if len(test_files) == 0:
      raise AssertionError(
          '%r matched no files, which was probably not intended' % glob_expr)
    for test_file in test_files:
      print 'Testing %s...' % test_file
      tests = test_format.LoadTestFile(test_file)
      tests = [Test(options, test) for test in tests]
      if options.update:
        test_format.SaveTestFile(tests, test_file)
      processed += 1
  print '%s test files were processed.' % processed


if __name__ == '__main__':
  main(sys.argv[1:])
