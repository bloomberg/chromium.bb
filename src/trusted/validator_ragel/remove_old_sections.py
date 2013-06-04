#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(shcherbina): delete this script after use.

import glob

import test_format


SECTIONS_TO_PRESERVE = [
    'hex',
    'dis', # content of this section should be replaced with nacl-objdump output
    'rdfa_output']


def main():
  for filename in glob.glob('src/trusted/validator_ragel/testdata/*/*.test'):
    tests = test_format.LoadTestFile(filename)
    for i in range(len(tests)):
      tests[i] = [
          (section, content) for section, content in tests[i]
          if section in SECTIONS_TO_PRESERVE]
    test_format.SaveTestFile(tests, filename)


if __name__ == '__main__':
  main()
