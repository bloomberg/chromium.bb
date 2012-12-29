#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

import gen_dfa


def main():
  def_filenames = sys.argv[1:]
  for filename in def_filenames:
    print 'Parsing %s...' % os.path.basename(filename)
    gen_dfa.ParseDefFile(filename)
  print 'Done.'


if __name__ == '__main__':
  main()
