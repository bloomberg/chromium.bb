#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Converts outputs from the old validator into a list of general errors.

The outputs then would be compared for exact match with what the
script run_ncval_tests.py outputs for the test.

Works under assumption that the corresponding input files (*.hex) maintain a
separation of instructions: one instruction per line in .hex file.  We could get
the same information from the validator, but, luckily, we do not have to do
that.
"""

import re
import sys

import run_ncval_tests


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


def ParseSfiBasedTest(test_name):
  # Parse the golden file 1st time, find bundle_crosses.
  bundle_crosses = []
  for line in open(test_name + '.rval', 'r').readlines():
    bundle_error_m = re.match(
        r'VALIDATOR: ERROR: ([0-9a-zA-Z]+): Bad basic block alignment', line)
    if bundle_error_m:
      bundle_crosses.append(int(bundle_error_m.group(1), 16))

  # Find offsets for all instructions that cross a bundle.
  err_offsets = {}
  insts = run_ncval_tests.InstByteSequence()
  insts.Parse(test_name + '.hex')
  for bundle_offset in set(bundle_crosses):
    off = bundle_offset - 1
    while off >= 0:
      if insts.HasOffset(off):
        err_offsets[off] = ['crosses boundary']
        break
      off -= 1

  # Parse the golden file 2nd time.  Find other offsets that cause errors.  An
  # error report takes 2 sequential lines.
  seen_offset = False
  offset = None
  for line in open(test_name + '.rval', 'r').readlines():
    ln = line.rstrip()
    off_m = re.match(r'VALIDATOR: ([0-9a-f]+):', ln)
    if off_m:
      seen_offset = True
      prev_offset = offset
      offset = int(off_m.group(1), 16)
      continue
    val_error_m = re.match(r'VALIDATOR: ERROR:', ln)
    if val_error_m and seen_offset and prev_offset != offset:
      err_offsets.setdefault(offset, []).append('validation error')
    seen_offset = False
  return err_offsets


def ParseSegmentBasedTest(test_name):
  err_offsets = {}
  for line in open(test_name + '.nval', 'r').readlines():
    ln = line.rstrip()
    off_m = re.compile(r'''VALIDATOR:[ ]
                       ([0-9a-f]+):[ ]  # offset
                       # Possible messages:
                       ((Illegal|Undefined)[ ]instruction|
                        Bad[ ]prefix[ ]usage|
                        Unsafe[ ]indirect[ ]jump
                       )
                       ''',
                       re.VERBOSE).match(ln)
    if off_m:
      offset = int(off_m.group(1), 16)
      if offset not in err_offsets:
        err_offsets[offset] = ['validation error']
    bundle_m = re.match(r'VALIDATOR: ([0-9a-f]+): Bad basic block alignment',
                        ln)
    if bundle_m:
      offset = int(bundle_m.group(1), 16)
      if test_name[12:] == 'crosses_block':
        # Ugly special case for the test 'crosses_block' that places the bundle
        # boundary immediately after the masking instruction of the indirect
        # call.  For the old validator it is an 'indivisible' sequence that is
        # divided.  For the DFA-based validator the error is only in the
        # following call that does not see the preceding masking operation.
        err_offsets[offset] = ['validation error']
        continue
      err_offsets.setdefault(offset, []).insert(0, 'crosses boundary')

  return err_offsets


def Main(argv):
  argv = argv[1:]
  for arg in argv:
    last_5 = arg[-5:]
    test = arg[:-5]
    if last_5 == '.rval':
      err_offsets = ParseSfiBasedTest(test)
    elif last_5 == '.nval':
      err_offsets = ParseSegmentBasedTest(test)
    else:
      raise Exception('bad file name: %s' % arg)

    # Output the error messages in offset order.
    golden_text = ''
    for off, msg_lst in sorted(err_offsets.iteritems()):
      for msg in msg_lst:
        golden_text += 'offset 0x%x: %s\n' % (off, msg)
    filename = test + '.val.ref'
    print 'writing file: %s' % filename
    WriteFile(filename, golden_text)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
