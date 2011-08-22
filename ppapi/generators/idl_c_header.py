#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import sys
import subprocess

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import CGen
from idl_generator import Generator

Option('dstroot', 'Base directory of output', default='../c')
Option('guard', 'Include guard prefix', default='ppapi/c')
Option('out', 'List of output files', default='')

cgen = CGen()

def IDLToHeader(filenode, relpath=None, prefix=None):
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0] + '.h'
  if prefix: name = '%s%s' % (prefix, name)
  if path: name = os.path.join(path, name)
  if relpath: name = os.path.join(relpath, name)
  return name


def GenerateHeader(filenode, release, pref, inline=True):
  name = filenode.GetProperty('NAME')
#  if name == 'pp_stdint.idl':  return

  # If we have an 'out' filter list, then check if we should output this file.
  outlist = GetOption('out')
  if outlist:
    outlist = outlist.split(',')
    if name not in outlist:
      return

  savename = IDLToHeader(filenode, relpath=GetOption('dstroot'), prefix=pref)
  out = IDLOutFile(savename)

  gpath = GetOption('guard')
  def_guard = IDLToHeader(filenode, relpath=gpath, prefix=pref)
  def_guard = def_guard.replace('/','_').replace('.','_').upper() + '_'
  cright_node = filenode.GetChildren()[0]
  assert(cright_node.IsA('Copyright'))

  fileinfo = filenode.GetChildren()[1]
  assert(fileinfo.IsA('Comment'))

  out.Write('%s\n' % cgen.Copyright(cright_node))
  out.Write('/* From %s modified %s. */\n\n'% (
      filenode.GetProperty('NAME'), filenode.GetProperty('DATETIME')))
  out.Write('#ifndef %s\n#define %s\n\n' % (def_guard, def_guard))

  for label in filenode.GetListOf('Label'):
    if label.GetName() == GetOption('label'):
      cgen.SetVersionMap(label)

  deps = filenode.GetDeps(release)
  # Generate set of includes
  includes = set([])
  for dep in deps:
    depfile = dep.GetProperty('FILE')
    if depfile:
      includes.add(depfile)
  includes = [IDLToHeader(include, relpath=gpath) for include in includes]
  includes.append('ppapi/c/pp_macros.h')

  # Assume we need stdint if we "include" C or C++ code
  if filenode.GetListOf('Include'):
    includes.append('ppapi/c/pp_stdint.h')

  includes = sorted(set(includes))
  cur_include = IDLToHeader(filenode, relpath=gpath)
  for include in includes:
    if include == cur_include: continue
    out.Write('#include "%s"\n' % include)

  # Generate all interface defines
  out.Write('\n')
  for node in filenode.GetListOf('Interface'):
    out.Write( cgen.InterfaceDefs(node) )

  # Generate the @file comment
  out.Write('%s\n' % cgen.Comment(fileinfo, prefix='*\n @file'))

  # Generate definitions.
  last_group = None
  for node in filenode.GetChildren()[2:]:
    # If we are part of a group comment marker...
    if last_group and last_group != node.cls:
      pre = cgen.CommentLines(['*',' @}', '']) + '\n'
    else:
      pre = '\n'

    if node.cls in ['Typedef', 'Interface', 'Struct', 'Enum']:
      if last_group != node.cls:
        pre += cgen.CommentLines(['*',' @addtogroup %ss' % node.cls, ' @{', ''])
      last_group = node.cls
    else:
      last_group = None

    if node.IsA('Comment'):
      item = '%s\n\n' % cgen.Comment(node)
    elif node.IsA('Inline'):
      if not inline: continue
      if node.GetName() == 'cc':
        item = cgen.Define(node, prefix=pref, comment=True)
        item = '#ifdef __cplusplus\n%s\n#endif  // __cplusplus\n\n' % item
      elif node.GetName() == 'c':
        item = cgen.Define(node, prefix=pref, comment=True)
      else:
        continue
      if not item: continue
    else:
      #
      # Otherwise we are defining a file level object, so generate the
      # correct document notation.
      #
      item = cgen.Define(node, prefix=pref, comment=True)
      if not item: continue
      asize = node.GetProperty('assert_size()')
      if asize:
        name = '%s%s' % (pref, node.GetName())
        if node.IsA('Struct'):
          form = 'PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(%s, %s);\n'
        else:
          form = 'PP_COMPILE_ASSERT_SIZE_IN_BYTES(%s, %s);\n'
        item += form % (name, asize[0])

    if item: out.Write('%s%s' % (pre, item))
  if last_group:
    out.Write(cgen.CommentLines(['*',' @}', '']) + '\n')

  out.Write('#endif  /* %s */\n\n' % def_guard)
  return out.Close()

class HGen(Generator):
  def __init__(self):
    Generator.__init__(self, 'C Header', 'cgen', 'Generate the C headers.')

  def GenerateVersion(self, ast, release, options):
    outdir = GetOption('dstroot')
    skipList= []
    prefix = ''
    cfile = None
    cnt = 0

    for filenode in ast.GetListOf('File'):
      if GetOption('verbose'):
        print "Working on %s" % filenode

      # If this file has errors, skip it
      if filenode.GetProperty('ERRORS') > 0:
        skipList.append(filenode)
        continue

      if GenerateHeader(filenode, release, prefix):
        cnt = cnt + 1

    for filenode in skipList:
      errcnt = filenode.GetProperty('ERRORS')
      ErrOut.Log('%s : Skipped because of %d errors.' % (
          filenode.GetName(), errcnt))

    if skipList:
      return -len(skipList)

    if GetOption('diff'):
      return -cnt
    return cnt


hgen = HGen()

def Main(args):
  # Default invocation will verify the golden files are unchanged.
  if not args:
    args = ['--wnone', '--diff', '--test', '--dstroot=.']

  ParseOptions(args)
  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_cgen', '*.idl')
  filenames = glob.glob(idldir)

  ast = ParseFiles(filenames)
  return hgen.GenerateVersion(ast, 'M14', {})

if __name__ == '__main__':
  retval = Main(sys.argv[1:])
  sys.exit(retval)

