#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import optparse
import subprocess
import sys

import test_format


FIELDS_TO_IGNORE = set(['rdfa_output', 'validators_disagree'])


def AssertEquals(actual, expected):
  if actual != expected:
    raise AssertionError('\nEXPECTED:\n"""\n%s"""\n\nACTUAL:\n"""\n%s"""'
                         % (expected, actual))


def GetX8632Combinations():
  for detailed in (True, False):
    for test_stats in (True, False):
      for test_cpuid_all in (True, False):
        ext = 'nval'
        options = []
        if detailed:
          ext += 'd'
          options.append('--detailed')
        else:
          options.append('--detailed=false')
        if test_stats:
          ext += 's'
          options.append('--stats')
        if test_cpuid_all:
          options.append('--cpuid-all')
        else:
          ext += '0'
          options.append('--cpuid-none')
        yield ext, options


def GetX8664Combinations():
  for test_readwrite in (True, False):
    for detailed in (True, False):
      for test_cpuid_all in (True, False):
        for test_annotate in (True, False):
          for validator_decoder in (True, False):
            options = []
            # TODO(shcherbina): Remove support for testing validation
            # without sandboxing memory reads.
            if test_readwrite:
              ext = 'rval'
              options.append('--readwrite_sfi')
            else:
              ext = 'val'
              options.append('--write_sfi')
            if detailed:
              ext += 'd'
              options.append('--detailed')
            else:
              options.append('--detailed=false')
            if test_cpuid_all:
              options.append('--cpuid-all')
            else:
              ext += '0'
              options.append('--cpuid-none')
            if test_annotate:
              ext += 'a'
              options.append('--annotate')
            else:
              options.append('--annotate=false')
            if validator_decoder:
              ext = "vd-" + ext
              options.append('--validator_decoder')
            yield ext, options


def Test(options, items_list):
  info = dict(items_list)
  handled_fields = set(['hex'])

  def RunCommandOnHex(field, command):
    handled_fields.add(field)
    if field in info:
      proc = subprocess.Popen(command,
                              stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE)
      # r'\\' is used as line continuation marker by
      # run_rdfa_validator_tests.py. Old validator does not care about line
      # partitioning, so we just remove the marker.
      stdout, stderr = proc.communicate(info['hex'].replace(r'\\', ''))
      assert proc.wait() == 0, (command, stdout, stderr)
      # Remove the carriage return characters that we get on Windows.
      stdout = stdout.replace('\r', '')
      print '  Checking %r field...' % field
      if options.update:
        if stdout != info[field]:
          print '  Updating %r field...' % field
          info[field] = stdout
      else:
        AssertEquals(stdout, info[field])

  ncdis = [options.ncdis, '--hex_text=-']
  RunCommandOnHex('dis', ncdis + ['--full_decoder'])
  RunCommandOnHex('vdis', ncdis + ['--validator_decoder'])

  ncval = [options.ncval, '--hex_text=-', '--max_errors=-1']
  if options.bits == 32:
    for ext, ncval_options in GetX8632Combinations():
      RunCommandOnHex(ext, ncval + ncval_options)
  elif options.bits == 64:
    for ext, ncval_options in GetX8664Combinations():
      RunCommandOnHex(ext, ncval + ncval_options)

    RunCommandOnHex('sval', ncval + ['--stubout'])
  else:
    raise AssertionError('Unknown architecture: %r' % options.bits)

  unhandled = set(info.keys()) - set(handled_fields) - FIELDS_TO_IGNORE
  if unhandled:
    raise AssertionError('Unhandled fields: %r' % sorted(unhandled))

  # Update field values, but preserve their order.
  items_list = [(field, info[field]) for field, _ in items_list]

  return items_list


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--ncval', default='ncval',
                    help='Path to the ncval validator executable')
  parser.add_option('--ncdis', default='ncdis',
                    help='Path to the ncdis disassembler executable')
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
