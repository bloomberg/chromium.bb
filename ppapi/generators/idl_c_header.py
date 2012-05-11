#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import CGen, GetNodeComments, CommentLines, Comment
from idl_generator import Generator, GeneratorByFile

Option('dstroot', 'Base directory of output', default=os.path.join('..', 'c'))
Option('guard', 'Include guard prefix', default=os.path.join('ppapi', 'c'))
Option('out', 'List of output files', default='')


def GetOutFileName(filenode, relpath=None, prefix=None):
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0] + '.h'
  if prefix: name = '%s%s' % (prefix, name)
  if path: name = os.path.join(path, name)
  if relpath: name = os.path.join(relpath, name)
  return name


def WriteGroupMarker(out, node, last_group):
  # If we are part of a group comment marker...
  if last_group and last_group != node.cls:
    pre = CommentLines(['*',' @}', '']) + '\n'
  else:
    pre = '\n'

  if node.cls in ['Typedef', 'Interface', 'Struct', 'Enum']:
    if last_group != node.cls:
      pre += CommentLines(['*',' @addtogroup %ss' % node.cls, ' @{', ''])
    last_group = node.cls
  else:
    last_group = None
  out.Write(pre)
  return last_group


def GenerateHeader(out, filenode, releases):
  cgen = CGen()
  pref = ''
  do_comments = True

  # Generate definitions.
  last_group = None
  top_types = ['Typedef', 'Interface', 'Struct', 'Enum', 'Inline']
  for node in filenode.GetListOf(*top_types):
    # Skip if this node is not in this release
    if not node.InReleases(releases):
      print "Skiping %s" % node
      continue

    # End/Start group marker
    if do_comments:
      last_group = WriteGroupMarker(out, node, last_group)

    if node.IsA('Inline'):
      item = node.GetProperty('VALUE')
      # If 'C++' use __cplusplus wrapper
      if node.GetName() == 'cc':
        item = '#ifdef __cplusplus\n%s\n#endif  // __cplusplus\n\n' % item
      # If not C++ or C, then skip it
      elif not node.GetName() == 'c':
        continue
      if item: out.Write(item)
      continue

    #
    # Otherwise we are defining a file level object, so generate the
    # correct document notation.
    #
    item = cgen.Define(node, releases, prefix=pref, comment=True)
    if not item: continue
    asize = node.GetProperty('assert_size()')
    if asize:
      name = '%s%s' % (pref, node.GetName())
      if node.IsA('Struct'):
        form = 'PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(%s, %s);\n'
      elif node.IsA('Enum'):
        if node.GetProperty('notypedef'):
          form = 'PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(%s, %s);\n'
        else:
          form = 'PP_COMPILE_ASSERT_SIZE_IN_BYTES(%s, %s);\n'
      else:
        form = 'PP_COMPILE_ASSERT_SIZE_IN_BYTES(%s, %s);\n'
      item += form % (name, asize[0])

    if item: out.Write(item)
  if last_group:
    out.Write(CommentLines(['*',' @}', '']) + '\n')


class HGen(GeneratorByFile):
  def __init__(self):
    Generator.__init__(self, 'C Header', 'cgen', 'Generate the C headers.')

  def GenerateFile(self, filenode, releases, options):
    savename = GetOutFileName(filenode, GetOption('dstroot'))
    unique_releases = filenode.GetUniqueReleases(releases)
    if not unique_releases:
      if os.path.isfile(savename):
        print "Removing stale %s for this range." % filenode.GetName()
        os.remove(os.path.realpath(savename))
      return False

    out = IDLOutFile(savename)
    self.GenerateHead(out, filenode, releases, options)
    self.GenerateBody(out, filenode, releases, options)
    self.GenerateTail(out, filenode, releases, options)
    return out.Close()

  def GenerateHead(self, out, filenode, releases, options):
    __pychecker__ = 'unusednames=options'
    cgen = CGen()
    gpath = GetOption('guard')
    def_guard = GetOutFileName(filenode, relpath=gpath)
    def_guard = def_guard.replace(os.sep,'_').replace('.','_').upper() + '_'

    cright_node = filenode.GetChildren()[0]
    assert(cright_node.IsA('Copyright'))
    fileinfo = filenode.GetChildren()[1]
    assert(fileinfo.IsA('Comment'))

    out.Write('%s\n' % cgen.Copyright(cright_node))

    # Wrap the From ... modified ... comment if it would be >80 characters.
    from_text = 'From %s' % (
        filenode.GetProperty('NAME').replace(os.sep,'/'))
    modified_text = 'modified %s.' % (
        filenode.GetProperty('DATETIME'))
    if len(from_text) + len(modified_text) < 74:
      out.Write('/* %s %s */\n\n' % (from_text, modified_text))
    else:
      out.Write('/* %s,\n *   %s\n */\n\n' % (from_text, modified_text))

    out.Write('#ifndef %s\n#define %s\n\n' % (def_guard, def_guard))
    # Generate set of includes

    deps = set()
    for release in releases:
      deps |= filenode.GetDeps(release)

    includes = set([])
    for dep in deps:
      depfile = dep.GetProperty('FILE')
      if depfile:
        includes.add(depfile)
    includes = [GetOutFileName(
        include, relpath=gpath).replace(os.sep, '/') for include in includes]
    includes.append('ppapi/c/pp_macros.h')

    # Assume we need stdint if we "include" C or C++ code
    if filenode.GetListOf('Include'):
      includes.append('ppapi/c/pp_stdint.h')

    includes = sorted(set(includes))
    cur_include = GetOutFileName(filenode, relpath=gpath).replace(os.sep, '/')
    for include in includes:
      if include == cur_include: continue
      out.Write('#include "%s"\n' % include)

    # If we are generating a single release, then create a macro for the highest
    # available release number.
    if filenode.GetProperty('NAME').endswith('pp_macros.idl'):
      releasestr = GetOption('release')
      if releasestr:
        release_numbers = re.findall('\d+', releasestr)
        if release_numbers:
          out.Write('\n#define PPAPI_RELEASE %s\n' % release_numbers[0])

    # Generate all interface defines
    out.Write('\n')
    for node in filenode.GetListOf('Interface'):
      idefs = ''
      macro = cgen.GetInterfaceMacro(node)
      unique = node.GetUniqueReleases(releases)

      # Skip this interface if there are no matching versions
      if not unique: continue

      for rel in unique:
        version = node.GetVersion(rel)
        name = cgen.GetInterfaceString(node, version)
        strver = str(version).replace('.', '_')
        idefs += cgen.GetDefine('%s_%s' % (macro, strver), '"%s"' % name)
      idefs += cgen.GetDefine(macro, '%s_%s' % (macro, strver)) + '\n'
      out.Write(idefs)

    # Generate the @file comment
    out.Write('%s\n' % Comment(fileinfo, prefix='*\n @file'))

  def GenerateBody(self, out, filenode, releases, options):
    __pychecker__ = 'unusednames=options'
    GenerateHeader(out, filenode, releases)

  def GenerateTail(self, out, filenode, releases, options):
    __pychecker__ = 'unusednames=options,releases'
    gpath = GetOption('guard')
    def_guard = GetOutFileName(filenode, relpath=gpath)
    def_guard = def_guard.replace(os.sep,'_').replace('.','_').upper() + '_'
    out.Write('#endif  /* %s */\n\n' % def_guard)


hgen = HGen()

def Main(args):
  # Default invocation will verify the golden files are unchanged.
  failed = 0
  if not args:
    args = ['--wnone', '--diff', '--test', '--dstroot=.']

  ParseOptions(args)

  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_cgen', '*.idl')
  filenames = glob.glob(idldir)
  ast = ParseFiles(filenames)
  if hgen.GenerateRelease(ast, 'M14', {}):
    print "Golden file for M14 failed."
    failed = 1
  else:
    print "Golden file for M14 passed."


  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_cgen_range', '*.idl')
  filenames = glob.glob(idldir)

  ast = ParseFiles(filenames)
  if hgen.GenerateRange(ast, ['M13', 'M14', 'M15'], {}):
    print "Golden file for M13-M15 failed."
    failed =1
  else:
    print "Golden file for M13-M15 passed."

  return failed

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

