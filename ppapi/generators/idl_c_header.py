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


def GenerateHeader(filenode, pref, inline=True):
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

  deps = filenode.GetDeps('M14')
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
  release = 'M14'
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
  out.Close()


def GenerateFileTest(cfile, filenode, pref):
  tests = []
  original = IDLToHeader(filenode, relpath='')
  savename = IDLToHeader(filenode, relpath='', prefix=pref)
  cfile.Write('/* Test %s */\n' % filenode.GetProperty('NAME'))
  cfile.Write('#include "%s"\n' % original)
  cfile.Write('#include "%s"\n' % savename)

  # For all children (excluding copyright notice)
  for node in filenode.children[1:]:
    # Test enums by assigning all enum items to themselves. Unfortunately this
    # will not catch cases where the '.h' has enums that the IDL does not.
    if node.IsA('Enum'):
      tests.append('test_%s' % node.GetProperty('NAME'))
      cfile.Write('int test_%s() {\n' % node.GetProperty('NAME'))
      cfile.Write('  int errors = 0;\n')
      for enum in node.GetListOf('EnumItem'):
        cfile.Write('  errors += VERIFY_ENUM(%s, %s%s);\n' %
            (enum.GetProperty('NAME'), pref, enum.GetProperty('NAME')))
      cfile.Write('  if (errors) printf("Failure in %s:%s.\\n");\n' %
          (filenode.GetName(), node.GetName()))
      cfile.Write('  return errors;\n}\n\n')
      continue

    # Use a structure asignment to verify equivilency
    if node.IsA('Interface', 'Struct'):
      tests.append('test_%s' % node.GetName())
      rtype1 = cgen.GetTypeName(node)
      rtype2 = cgen.GetTypeName(node, prefix='tst_')
      cfile.Write('int test_%s() {\n' % node.GetName())
      cfile.Write('  int errors = 0;\n');
      cmp_structs = '  %s A;\n  %s B;\n' % (rtype1, rtype2)
      cfile.Write(cmp_structs)
      cfile.Write('  memset(&A, 0, sizeof(A));\n')
      cfile.Write('  memset(&B, 1, sizeof(B));\n')
      for member in node.GetListOf('Member', 'Function'):
        if member.GetOneOf('Array'):
          cfile.Write('  memcpy(A.%s, B.%s, sizeof(A.%s));\n' %
                      (member.GetName(), member.GetName(), member.GetName()))
        else:
          cfile.Write('  A.%s = B.%s;\n' % (member.GetName(), member.GetName()))
      cfile.Write('  errors += (sizeof(A) != sizeof(B)) ? 1 : 0;\n')
      cfile.Write('  errors += (memcmp(&A, &B, sizeof(A))) ? 1 : 0;\n')
      cfile.Write('  return (sizeof(A) == sizeof(B));\n}\n')
      continue

  cfile.Write('\n\n')
  return tests

def GenerateBaseTest(cfile):
  for inc in ['<stdio.h>','<string.h>','"pp_stdint.h"']:
    cfile.Write('#include %s\n' % inc)
  cfile.Write('\n')
  cfile.Write('#define VERIFY_ENUM(orig, idl) Check(#orig, orig, idl)\n\n')
  cfile.Write('int Check(const char *name, int v1, int v2) {\n')
  cfile.Write('  if (v1 == v2) return 0;\n')
  cfile.Write('  printf("Mismatch enum %s : %d vs %d\\n", name, v1, v2);\n')
  cfile.Write('  return 1;\n}\n\n')


def Main(args):
  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)
  testFuncs = []
  skipList = []

  outdir = GetOption('dstroot')

  if GetOption('test'):
    prefix = 'tst_'
    cfile = IDLOutFile(os.path.join(outdir, 'test.c'))
    GenerateBaseTest(cfile)
  else:
    prefix = ''
    cfile = None

  for filenode in ast.GetListOf('File'):
    if GetOption('verbose'):
      print "Working on %s" % filenode

    # If this file has errors, skip it
    if filenode.GetProperty('ERRORS') > 0:
      skipList.append(filenode)
      continue

    GenerateHeader(filenode, prefix, inline=not cfile)
    if cfile: GenerateFileTest(cfile, filenode, prefix)

  if cfile:
    cfile.Write('int main(int argc, const char *args[]) {\n')
    cfile.Write('  int errors = 0;\n');
    for testname in testFuncs:
      cfile.Write('  errors += %s();\n' % testname)

    cfile.Write('  printf("Found %d error(s) in the IDL.\\n", errors);\n')
    cfile.Write('  return errors;\n}\n\n')
    cfile.Close()

  # TODO(noelallen) Add a standard test
    if not skipList:
      args = ['gcc', '-o', 'tester', '%s/test.c' % outdir, '-I%s' % outdir,
              '-I../c', '-I../..', '-DPPAPI_INSTANCE_REMOVE_SCRIPTING']
      InfoOut.Log('Running: %s' % ' '.join(args))
      ret = subprocess.call(args)
      if ret:
        ErrOut.Log('Failed to compile.')
        return -1
      InfoOut.Log('Running: ./tester')
      ret = subprocess.call('./tester')
      if ret > 0:
        ErrOut.Log('Found %d errors.' % ret)
        return ret
      InfoOut.Log('Success!')
      return 0

  for filenode in skipList:
    errcnt = filenode.GetProperty('ERRORS')
    ErrOut.Log('%s : Skipped because of %d errors.' % (
        filenode.GetName(), errcnt))

  if not skipList:
    InfoOut.Log('Processed %d files.' % len(ast.GetListOf('File')))
  return len(skipList)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

