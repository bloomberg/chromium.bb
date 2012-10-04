#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(shcherbina): remove this script when @rdfa_output field is added
# to all tests.

# This adds an empty @rdfa_output section at the end of files that don't contain
# this section.  The contents of this section can then be filled out by
# regenerating the golden files.

import glob


def main():
  for test_filename in glob.glob('../testdata/*/*.test'):
    with open(test_filename) as file_in:
      content = file_in.read()
    if '@rdfa_output:\n' in content:
      continue
    with open(test_filename, 'w') as file_out:
      file_out.write(content)
      file_out.write('@rdfa_output:\n')


if __name__ == '__main__':
  main()
