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


def Main(argv):
  argv = argv[1:]
  offset = None
  for arg in argv:
    assert(arg[-5:] == '.rval')

    # Parse the golden file 1st time, find bundle_crosses.
    bundle_crosses = []
    for line in open(arg, 'r').readlines():
      bundle_error_m = re.match(
          r'VALIDATOR: ERROR: ([0-9a-zA-Z]+): Bad basic block alignment', line)
      if bundle_error_m:
        bundle_crosses.append(int(bundle_error_m.group(1), 16))

    # Find offsets for all instructions that cross a bundle.
    err_offsets = {}
    insts = run_ncval_tests.InstByteSequence()
    insts.Parse(arg[:-5] + '.hex')
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
    for line in open(arg, 'r').readlines():
      ln = line.rstrip()
      off_m = re.match(r'VALIDATOR: ([0-9a-z]+):', ln)
      if off_m:
        seen_offset = True
        prev_offset = offset
        offset = int(off_m.group(1), 16)
        continue
      val_error_m = re.match(r'VALIDATOR: ERROR:', ln)
      if val_error_m and seen_offset and prev_offset != offset:
        err_offsets.setdefault(offset, []).append('validation error')
      seen_offset = False

    # Output the error messages in offset order.
    golden_text = ''
    for off, msg_lst in sorted(err_offsets.iteritems()):
      for msg in msg_lst:
        golden_text += 'offset 0x%x: %s\n' % (off, msg)
    filename = arg[:-5] + '.val.ref'
    print 'writing file: %s' % filename
    WriteFile(filename, golden_text)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
