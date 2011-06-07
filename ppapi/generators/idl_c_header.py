#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLAst, IDLNode
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import *

Option('hdir', 'Output directory', default='hdir')

def Main(args):
  filenames = ParseOptions(args)
  ast_result = ParseFiles(filenames)
  ast = ast_result.out

  ast.Dump()
  for filenode in ast.GetListOf('File'):
    savename = os.path.join(GetOption('hdir'), filenode.name)
    out = IDLOutFile(savename)

    # Write the include guard, copyright, and file comment.
    def_guard = filenode.name.replace('/','_').replace('.','_').upper()
    out.Write('#ifndef %s\n#define %s\n\n' % (def_guard, def_guard))
    out.Write('%s\n' % filenode.GetOneOf('Copyright').name)
    for comment in filenode.GetListOf('Comment'):
     out.Write('\n%s\n' % comment.name)

    # Generate definitions.
    for node in filenode.childlist[2:]:
      item = Define(node, prefix='tst_')
      print '>>%s<<' % item
      out.Write(item)

    out.Write('\n#endif /* %s */\n\n' % def_guard)
    out.Close()

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

