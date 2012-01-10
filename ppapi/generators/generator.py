#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

# Note: some of these files are imported to register cmdline options.
from idl_generator import Generator
from idl_option import ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_header import HGen
from idl_gen_pnacl import PnaclGen


def Main():
  args = sys.argv[1:]
  # If no arguments are provided, assume we are tring to rebuild the
  # C headers with warnings off.
  if not args:
    args = ['--wnone', '--cgen', '--range=start,end']

  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)
  return Generator.Run(ast)


if __name__ == '__main__':
  sys.exit(Main())
