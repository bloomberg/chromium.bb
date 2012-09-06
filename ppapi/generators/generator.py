#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
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
  # If no arguments are provided, assume we are trying to rebuild the
  # C headers with warnings off.
  if not args:
    args = [
        '--wnone', '--cgen', '--range=start,end',
        '--pnacl', '--pnaclshim',
        '../native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.c',
    ]
    current_dir = os.path.abspath(os.getcwd())
    script_dir = os.path.abspath(os.path.dirname(__file__))
    if current_dir != script_dir:
      print '\nIncorrect CWD, default run skipped.'
      print 'When running with no arguments set CWD to the scripts directory:'
      print '\t' + script_dir + '\n'
      print 'This ensures correct default paths and behavior.\n'
      return 1

  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)
  return Generator.Run(ast)


if __name__ == '__main__':
  sys.exit(Main())
