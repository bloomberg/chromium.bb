#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import subprocess
import sys


# TODO(shcherbina): Remove this script.  I'm just checking it in to
# provide a record of how the validator test files were converted.


TESTS_DIR = 'src/trusted/validator_x86/testdata'


# File extensions in the order we want them to appear in the converted
# *.test files.
KNOWN_EXTENSIONS = (
    'hex',
    # Validator files for checking the output of 32-bit ncval
    'nval',
    'nvals',
    'nvald',
    'nvalds',
    'snval',
    'nval0',
    # Validator files for checking the output of ncval
    'val',
    'rval',
    'rvala',
    'rvald',
    'cval',
    'cvale',
    'vd-rval',
    'sval',
    'val0',
    # Disassembly files for checking the output of ncdis
    'dis',
    'vdis',
    'ndis',
    )


# These files do not have corresponding *.hex files.
FILES_TO_IGNORE = (
    'src/trusted/validator_x86/testdata/32/fib_array.nexe',
    'src/trusted/validator_x86/testdata/32/fib_scalar.nexe',
    'src/trusted/validator_x86/testdata/32/ncdis_examples1.input',
    'src/trusted/validator_x86/testdata/32/ncdis_examples1.internal',
    'src/trusted/validator_x86/testdata/32/ncdis_examples1.vinput',
    'src/trusted/validator_x86/testdata/32/ncdis_examples1.vinternal',
    'src/trusted/validator_x86/testdata/32/ncdis_examples2.input',
    'src/trusted/validator_x86/testdata/32/ncdis_examples2.vinput',
    'src/trusted/validator_x86/testdata/32/null.nexe',
    'src/trusted/validator_x86/testdata/64/ncdis_examples.input',
    'src/trusted/validator_x86/testdata/64/ncdis_examples.internal',
    'src/trusted/validator_x86/testdata/64/ncdis_examples.vinput',
    'src/trusted/validator_x86/testdata/64/ncdis_examples.vinternal',
    )


def main(args):
  do_git = False
  if '--git' in args:
    args.remove('--git')
    do_git = True

  assert len(args) == 1, args
  output_dir = args[0]

  for bits in (32, 64):
    all_files = sorted(glob.glob('%s/%s/*.*' % (TESTS_DIR, bits)))
    pairs = []
    hex_tests = []
    for test_file in all_files:
      basename = os.path.basename(test_file)
      assert '.' in basename, basename
      name, ext = basename.split('.', 1)
      pairs.append((name, ext))
      if ext == 'hex':
        hex_tests.append(name)

    handled_files = set()
    to_add = []
    to_remove = []
    for name in sorted(hex_tests):
      new_file = '%s/%s/%s.test' % (output_dir, bits, name)
      handled_files.add(new_file)
      to_add.append(new_file)
      fh = open(new_file, 'w')
      parts = [ext for name2, ext in pairs if name2 == name]
      for part in parts:
        assert part in KNOWN_EXTENSIONS, part
      # Sort parts by the order they appear in KNOWN_EXTENSIONS.
      parts = [part for part in KNOWN_EXTENSIONS
               if part in parts]
      for part in parts:
        part_file = '%s/%s/%s.%s' % (TESTS_DIR, bits, name, part)
        if part_file in all_files:
          handled_files.add(part_file)
          to_remove.append(part_file)
          fh.write('@%s:\n' % part)
          for line in open(part_file):
            fh.write('  %s\n' % line.rstrip('\n'))
      fh.close()

    # Don't delete these files yet because 'neg_max_errs' test refers to
    # them directly.
    if bits == 32:
      to_remove.remove('src/trusted/validator_x86/testdata/32/ret.hex')
      to_remove.remove('src/trusted/validator_x86/testdata/32/ret.nval')

    if do_git:
      subprocess.check_call(['git', 'rm'] + to_remove)
      subprocess.check_call(['git', 'add'] + to_add)
    unhandled_files = set(all_files).difference(handled_files).difference(
        FILES_TO_IGNORE)
    assert len(unhandled_files) == 0, unhandled_files


if __name__ == '__main__':
  main(sys.argv[1:])
