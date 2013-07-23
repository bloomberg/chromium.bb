#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(shcherbina): delete this script after use.

import collections
import glob
import os
import re
import StringIO

import test_format


ValidatorResult = collections.namedtuple('ValidatorResult', 'verdict offsets')


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


def ParseSpec(spec_content):
  if spec_content == 'SAFE\n':
    return ValidatorResult(verdict=True, offsets=set())

  offsets = set()
  for line in StringIO.StringIO(spec_content):
    m = re.match(r'([0-9a-f]+): ', line)
    offsets.add(int(m.group(1), 16))

  return ValidatorResult(verdict=False, offsets=offsets)


def main():
  sink = StringIO.StringIO()

  with open('minor_differences.txt', 'w') as minor_differences,\
       open('major_differences.txt', 'w') as major_differences:
    for filename in sorted(
        glob.glob('src/trusted/validator_ragel/testdata/*/*.test')):
      tests = test_format.LoadTestFile(filename)

      has_minor = False
      has_major = False

      for i, test in enumerate(tests):
        t = dict(test)
        if 'spec' in t:
          rdfa_result = ParseRdfaOutput(t['rdfa_output'])
          spec_result = ParseSpec(t['spec'])

          if rdfa_result.verdict != spec_result.verdict:
            out = major_differences
            has_major = True
          elif rdfa_result != spec_result:
            out = minor_differences
            has_minor = True
          else:
            out = sink

          out.write('-----------------\n')
          out.write('### %s, testcase %d\n' % (filename, i))
          for line in test_format.UnparseTest(test):
            out.write(line)

      if has_major:
        print 'file %s has major differences' % filename
      elif has_minor:
        print 'file %s has minor differences' % filename
      else:
        print 'file %s is clean' % filename
        os.system('git add %s' % filename)


if __name__ == '__main__':
  main()
