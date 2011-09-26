#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from idl_ast import IDLAst
from idl_generator import Generator
from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_header import HGen


def Main(args):
  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)
  return Generator.Run(ast)

if __name__ == '__main__':
  args = sys.argv[1:]

  # If no arguments are provided, assume we are tring to rebuild the
  # C headers with warnings off.
  if not args: args = ['--wnone', '--cgen', '--range=M13,M16']

  sys.exit(Main(args))

