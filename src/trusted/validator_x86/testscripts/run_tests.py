#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import optparse
import re
import subprocess
import sys


def AssertEquals(actual, expected):
  if actual != expected:
    raise AssertionError('\nEXPECTED:\n"""\n%s"""\n\nACTUAL:\n"""\n%s"""'
                         % (expected, actual))


def ParseTestFile(filename):
  fh = open(filename, 'r')
  fields = {}
  current_field = None
  for line in fh:
    if line.startswith('  '):
      assert current_field is not None, line
      fields[current_field].append(line[2:])
    else:
      match = re.match('@(\S+):$', line)
      if match is None:
        raise Exception('Bad line: %r' % line)
      current_field = match.group(1)
      assert current_field not in fields, current_field
      fields[current_field] = []
  return dict((key, ''.join(value)) for key, value in fields.iteritems())


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
              options.append('--annotate');
            else:
              options.append('--annotate=false');
            if validator_decoder:
              ext = "vd-" + ext
              options.append('--validator_decoder')
            yield ext, options


def Test(options, info):
  handled_fields = set(['hex'])

  def RunCommandOnHex(field, command):
    handled_fields.add(field)
    if field in info:
      proc = subprocess.Popen(command,
                              stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE)
      stdout, stderr = proc.communicate(info['hex'])
      assert proc.wait() == 0, (command, stdout, stderr)
      # Remove the carriage return characters that we get on Windows.
      stdout = stdout.replace('\r', '')
      print '  Checking %r field...' % field
      AssertEquals(stdout, info[field])

  ncdis = [options.ncdis, '--hex_text=-']
  RunCommandOnHex('dis', ncdis + ['--full_decoder'])
  RunCommandOnHex('vdis', ncdis + ['--validator_decoder'])

  ncval = [options.ncval, '--hex_text=-', '--max_errors=-1']
  if options.bits == '32':
    # TODO(shcherbina): no tests read snval
    if 'snval' in info:
      del info['snval']

    for ext, options in GetX8632Combinations():
      RunCommandOnHex(ext, ncval + options)
  elif options.bits == '64':
    for ext, options in GetX8664Combinations():
      RunCommandOnHex(ext, ncval + options)

    RunCommandOnHex('sval', ncval + ['--stubout'])

    # TODO(shcherbina): no tests read ndis
    if 'ndis' in info:
      del info['ndis']
  else:
    raise AssertionError('Unknown architecture: %r' % options.bits)

  unhandled = set(info.keys()).difference(handled_fields)
  if unhandled:
    raise AssertionError('Unhandled fields: %r' % sorted(unhandled))


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--ncval', default='ncval',
                    help='Path to the ncval validator executable')
  parser.add_option('--ncdis', default='ncdis',
                    help='Path to the ncdis disassembler executable')
  parser.add_option('--bits',
                    help='The subarchitecture to run tests against: 32 or 64')
  options, args = parser.parse_args()
  if options.bits is None:
    parser.error('--bits argument missing')
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
      Test(options, ParseTestFile(test_file))
      processed += 1
  print '%s test files were processed.' % processed


if __name__ == '__main__':
  main(sys.argv[1:])
