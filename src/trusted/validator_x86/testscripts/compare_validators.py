#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import glob
import optparse
import re
import subprocess
import sys

import test_format


NCVAL_VERDICT = {
    '*** <input> is safe ***': True,
    '*** <input> IS UNSAFE ***': False}


ValidatorResult = collections.namedtuple('ValidatorResult', 'verdict offsets')


def ParseNval(nval_content):
  """Parse content of @nval section.

  Args:
    nval_content: Content of @nval section (as produced by 32-bit ncval
        with options --max_errors=-1 --detailed=false --cpuid-all).

  Returns:
    ValidatorResult
  """

  lines = nval_content.split('\n')
  last_line = lines.pop()
  assert last_line == ''
  verdict = NCVAL_VERDICT[lines.pop()]

  offsets = set()
  for line in lines:
    if re.match(r'.+ > .+ \(read overflow of .+ bytes\)', line):
      # Add to offsets something that designedly can't occur in offsets
      # produced by ParseRdfaOutput, so that difference will show up and
      # corresponding test won't be missed.
      offsets.add('read overflow')
      continue
    if line == 'ErrorSegmentation':
      # Same here.
      offsets.add('ErrorSegmentation')
      continue

    # Parse error message of the form
    #   VALIDATOR: 4: Bad prefix usage
    m = re.match(r'VALIDATOR: ([0-9a-f]+): (.*)$', line, re.IGNORECASE)

    assert m is not None, "can't parse %r" % line

    offset = int(m.group(1), 16)
    offsets.add(offset)

  return ValidatorResult(verdict=verdict, offsets=offsets)


def ParseRval(rval_content):
  """Parse content of @rval section.

  Args:
    nval_content: Content of @rval section (as produced by 64-bit ncval
        with options --max_errors=-1 --readwrite_sfi --detailed=false
        --annotate=false --cpuid-all).

  Returns:
    ValidatorResult
  """

  lines = rval_content.split('\n')
  last_line = lines.pop()
  assert last_line == ''
  verdict = NCVAL_VERDICT[lines.pop()]

  offsets = set()
  for prev_line, line in zip([None] + lines, lines):
    if line.startswith('VALIDATOR: Checking jump targets:'):
      continue
    if line.startswith('VALIDATOR: Checking that basic blocks are aligned'):
      continue

    # Skip disassembler output of the form
    #   VALIDATOR: 0000000000000003: 49 89 14 07    mov [%r15+%rax*1], %rdx
    m = re.match(r'VALIDATOR: ([0-9a-f]+):', line, re.IGNORECASE)
    if m is not None:
      continue

    # Parse error message of the form
    #   VALIDATOR: ERROR: 20: Bad basic block alignment.
    m = re.match(r'VALIDATOR: ERROR: ([0-9a-f]+): (.*)', line, re.IGNORECASE)
    if m is not None:
      offset = int(m.group(1), 16)
      offsets.add(offset)
      continue

    # Parse two-line error messages of the form
    #   VALIDATOR: 0000000000000003: 49 89 14 07    mov [%r15+%rax*1], %rdx
    #   VALIDATOR: ERROR: Invalid index register in memory offset
    m = re.match(r'VALIDATOR: (ERROR|WARNING): .*$', line, re.IGNORECASE)
    if m is not None:
      message_type = m.group(1)
      assert prev_line is not None, (
          "can't deduce error offset because line %r "
          "is not preceded with disassembly" % line)
      m2 = re.match(r'VALIDATOR: ([0-9a-f]+):', prev_line, re.IGNORECASE)
      assert m2 is not None, "can't parse line %r preceding line %r" % (
          prev_line,
          line)
      offset = int(m2.group(1), 16)
      if message_type != 'WARNING':
        offsets.add(offset)
      continue

    raise AssertionError("can't parse line %r" % line)

  return ValidatorResult(verdict=verdict, offsets=offsets)


RDFA_VERDICT = {
    'return code: 0': True,
    'return code: 1': False}


def ParseRdfaOutput(rdfa_content):
  """Parse content of @rdfa_output section.

  Args:
    rdfa_content: Content of @rdfa_output section in .test file.

  Returns:
    ValidatorResult
  """

  lines = rdfa_content.split('\n')
  assert lines[-1] == ''
  verdict = RDFA_VERDICT[lines[-2]]

  offsets = set()
  for line in lines[:-2]:
    # Parse error message of the form
    #   4: [1] DFA error in validator
    m = re.match(r'([0-9a-f]+): \[\d+\] (.*)$', line, re.IGNORECASE)
    assert m is not None, "can't parse %r" % line
    offset = int(m.group(1), 16)
    offsets.add(offset)

  return ValidatorResult(verdict=verdict, offsets=offsets)


def Compare(options, items_list, stats):
  val_field = {32: 'nval', 64: 'rval'}[options.bits]
  val_parser = {32: ParseNval, 64: ParseRval}[options.bits]

  info = dict(items_list)
  if 'rdfa_output' not in info:
    if val_field in info:
      print '  rdfa_output section is missing'
      stats['rdfa missing'] +=1
    else:
      print '  both sections are missing'
      stats['both missing'] += 1
    return items_list
  if val_field not in info:
    print '  @%s section is missing' % val_field
    stats['val missing'] += 1
    return items_list

  val = val_parser(info[val_field])
  rdfa = ParseRdfaOutput(info['rdfa_output'])

  if val == rdfa:
    stats['agree'] += 1
    if options.git:
      # Stage the file for commit, so that files that pass can be
      # committed with "git commit" (without -a or other arguments).
      subprocess.check_call(['git', 'add', test_filename])

    if 'validators_disagree' in info:
      stats['spurious @validators_disagree'] += 1
      print ('  warning: validators agree, but the section '
             '"@validators_disagree" is present')
  else:
    stats['disagree'] += 1
    if 'validators_disagree' in info:
      print '  validators disagree, see @validators_disagree section'
    else:
      print '  validators disagree!'
      stats['unexplained disagreements'] += 1

      diff = ['TODO: explain this\n']
      if val.verdict != rdfa.verdict:
        diff.append('old validator verdict: %s\n' % val.verdict)
        diff.append('rdfa validator verdict: %s\n' % rdfa.verdict)

      set_diff = val.offsets - rdfa.offsets
      if len(set_diff) > 0:
        diff.append('errors reported by old validator but not by rdfa one:\n')
        for offset in sorted(set_diff):
          if isinstance(offset, int):
            offset = hex(offset)
          diff.append('  %s\n' % offset)

      set_diff = rdfa.offsets - val.offsets
      if len(set_diff) > 0:
        diff.append('errors reported by rdfa validator but not by old one:\n')
        for offset in sorted(set_diff):
          if isinstance(offset, int):
            offset = hex(offset)
          diff.append('  %s\n' % offset)

      items_list = items_list + [('validators_disagree', ''.join(diff))]

  return items_list


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--bits',
                    type=int,
                    help='The subarchitecture to run tests against: 32 or 64')
  parser.add_option('--update',
                    default=False,
                    action='store_true',
                    help='When validators disagree, fill in '
                         '@validators_disagree section (if not present)')
  # TODO(shcherbina): Get rid of this option once most tests are committed.
  parser.add_option('--git',
                    default=False,
                    action='store_true',
                    help='Add tests with no discrepancies to git index')

  options, args = parser.parse_args(args)

  if options.bits not in [32, 64]:
    parser.error('specify --bits 32 or --bits 64')

  if len(args) == 0:
    parser.error('No test files specified')

  stats = collections.defaultdict(int)

  for glob_expr in args:
    test_files = sorted(glob.glob(glob_expr))
    if len(test_files) == 0:
      raise AssertionError(
          '%r matched no files, which was probably not intended' % glob_expr)
    for test_file in test_files:
      print 'Comparing', test_file
      tests = test_format.LoadTestFile(test_file)
      tests = [Compare(options, test, stats) for test in tests]
      if options.update:
        test_format.SaveTestFile(tests, test_file)

  print 'Stats:'
  for key, value in stats.items():
    print '  %s: %d' %(key, value)

  if options.update:
    if stats['unexplained disagreements'] > 0:
      print '@validators_disagree sections were created'
  else:
    if stats['unexplained disagreements'] > 0:
      sys.exit(1)


if __name__ == '__main__':
  main(sys.argv[1:])
