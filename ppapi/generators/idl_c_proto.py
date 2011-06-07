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
from idl_parser import IDLParser, ParseFiles


#
# 'C' style parameter and return styles
#

# Normal parameters (int8_t, etc...)
NormalType = {
  'in': '$TYPEREF$',
  'inout': '$TYPEREF$*',
  'out': '$TYPEREF$*',
  'store': '$TYPEREF$',
  'return': '$TYPEREF$'
}

# Enum uses the enum's name, except when storing.
EnumType = {
  'in': '$TYPEREF$',
  'inout': '$TYPEREF$*',
  'out': '$TYPEREF$*',
  'store': 'int',
  'return': '$TYPEREF$'
}

# A mem_t is a 'void *' to memory.
MemPtrType = {
  'in': 'const void*',
  'inout': 'void*',
  'out': 'void*',
  'store': 'void*',
  'return': 'void*'
}

# A str_t is a string pointer 'char *'.
CharPtrType = {
  'in': 'const char*',
  'inout': 'char*',
  'out': 'char*',
  'return': 'char*',
  'store': 'char*'
}

# A function pointer type
FuncType = {
  'in': '$TYPEREF$',
  'inout': '$TYPEREF$',
  'out': '$TYPEREF$',
  'return': '$TYPEREF$',
  'store': '$TYPEREF$'
}

# A 'struct' is passed by pointer.
StructType = {
  'in': 'const $TYPEREF$*',
  'inout': '$TYPEREF$*',
  'out': '$TYPEREF$*',
  'return': '$TYPEREF$*',
  'store': '$TYPEREF$'
}

# A 'void' does not have a parameter or storage type.
VoidType = {
  'in': None,
  'inout': None,
  'out': None,
  'return': 'void',
  'store': None
}

TypeMap = {
  'func': FuncType,
  'enum': EnumType,
  'mem_t': MemPtrType,
  'str_t': CharPtrType,
  'struct': StructType,
  'void': VoidType
}


# Place the individual 'normal' types in the type map.
for name in ['int8_t', 'int16_t' ,'int32_t', 'int64_t', 'uint8_t', 'uint16_t',
             'uint32_t', 'uint64_t', 'double_t', 'float_t', 'handle_t', 'bool']:
  TypeMap[name] = NormalType



# Return the name of the node with suffix/prefix if availible.
def GetName(node, **keyargs):
  prefix = keyargs.get('prefix','')
  suffix = keyargs.get('suffix', '')
  named = keyargs.get('named', True)
  name = node.name
  if not named: name = ''
  return '%s%s%s' % (prefix, name, suffix)


# Return the array specification of the object.
def GetArrayspec(node, name):
  assert(node.cls == 'Array')
  out = ''
  fixed = node.GetProperty('FIXED')
  if fixed:
    out = '%s[%s]' % (name, fixed)
  else:
    out = '*%s' % name
  # Add children
  for child in node.GetListOf('Array'):
    out += GetArrayspec(child, name)
  return out


# Return the <name>[<size>] form of the node.
def GetNameArray(node, **keyargs):
  name = GetName(node, **keyargs)
  for child in node.GetListOf('Array'):
    name = GetArrayspec(child, name)
  return name


def GetRootType(node):
  root = node
  while root.typeinfo:
    typeinfo = root.typeinfo
    if root.GetOneOf('Callspec'):
      return TypeMap['func']
    root = root.typeinfo
  return TypeMap[root.name]


# Get the passing form of the parameter.
def GetParamType(node, **keyargs):
  typedata = GetRootType(node)
  if node.GetProperty('in'):
    return node.Replace(typedata['in'])
  if node.GetProperty('out'):
    return node.Replace(typedata['out'])
  if node.GetProperty('inout'):
    return node.Replace(typedata['inout'])
  return node.GetProperty('TYPEREF')


# GetParam
#
# A Param signature is different than that of a Member because a Member is
# stored in the structure as declared, but the Param's signature is affected
# by how it gets passed and it's extended attributes like IN, OUT, and INOUT.
#
def GetParam(node, **keyargs):
  # This may be a function.
  callnode = node.GetOneOf('Callspec')

  typeref  = GetParamType(node, **keyargs)
  name = GetNameArray(node, **keyargs)
  if callnode:
    arglist = GetParamList(callnode, **keyargs)
    return '%s (*%s)(%s)' % (typeref, name, ', '.join(arglist))
  return '%s %s' % (typeref, name)


# GetParamList
def GetParamList(node, **keyargs):
  assert(node.cls == 'Callspec')
  out = []
  for child in node.GetListOf('Param'):
    out.append(GetParam(child, **keyargs))
  return out


# DefineSignature
def DefineSignature(node, **keyargs):
  # This may be a function.
  callnode = node.GetOneOf('Callspec')

  typeref  = node.GetProperty('TYPEREF')
  name = GetNameArray(node, **keyargs)
  if callnode:
    arglist = GetParamList(callnode, **keyargs)
    return '%s (*%s)(%s)' % (typeref, name, ', '.join(arglist))
  return '%s %s' % (typeref, name)

# Define a Member (or Function) in an interface.
def DefineMember(node, **keyargs):
  return '%s;\n' % DefineSignature(node, **keyargs)

# Define an Typedef.
def DefineTypedef(node, **keyargs):
  return 'typedef %s;\n' % DefineSignature(node, **keyargs)

# Define an Enum.
def DefineEnum(node, **keyargs):
  out = 'enum %s {' % GetName(node, **keyargs)

  enumlist = []
  for child in node.GetListOf('EnumItem'):
    value = child.GetProperty('VALUE')
    enumlist.append('  %s = %s' % (GetName(child), value))
  return '%s\n%s\n};\n' % (out, ',\n'.join(enumlist))

# Define a Struct.
def DefineStruct(node, **keyargs):
  out = 'struct %s {\n' % (GetName(node, **keyargs))

  # Generate Member Functions
  for child in node.GetListOf('Function'):
    out += '  %s' % DefineMember(child, **keyargs)

  # Generate Member Variables
  for child in node.GetListOf('Member'):
    out += '  %s' % DefineMember(child, **keyargs)
  out += '};'
  return out


# Define a top level object.
def Define(node, **keyargs):
  declmap = {
    'Enum' : DefineEnum,
    'Interface' : DefineStruct,
    'Struct' : DefineStruct,
    'Typedef' : DefineTypedef,
  }

  func = declmap.get(node.cls)
  if not func:
    ErrLog.Log('Failed to define %s named %s' % (node.cls, node.name))
  out = func(node, **keyargs)

  tab = ''
  for i in range(keyargs.get('tabs', 0)):
    tab += '  '

  return ('%s\n' % tab).join(out.split('\n')) + '\n'


# Clean a string representing an object definition and return then string
# as a single space delimited set of tokens.
def CleanString(instr):
  instr = instr.strip()
  instr = instr.split()
  return ' '.join(instr)


# Test a file, by comparing all it's objects, with their comments.
def TestFile(filenode):
  errors = 0
  for node in filenode.Children()[2:]:
    if GetOption('verbose'):
      node.Dump(0, comments=True)

    instr = node.GetOneOf('Comment')
    instr = CleanString(instr.name)[3:-3]

    outstr = Define(node)
    outstr = CleanString(outstr)

    if instr != outstr:
      ErrOut.Log('Failed match of\n>>%s<<\n>>%s<<\nto:' % (instr, outstr))
      node.Dump(1, comments=True)
      errors += 1
  return errors



# Build and resolve the AST and compare each file individual.
def TestFiles(filenames):
  if not filenames:
    idldir = os.path.split(sys.argv[0])[0]
    idldir = os.path.join(idldir, 'test_cgen', '*.idl')
    filenames = glob.glob(idldir)

  filenames = sorted(filenames)
  ast_result = ParseFiles(filenames)

  total_errs = 0
  for filenode in ast_result.out.GetListOf('File'):
    errs = TestFile(filenode)
    if errs:
      ErrOut.Log('%s test failed with %d error(s).' % (filenode.name, errs))
      total_errs += errs

  if total_errs:
    ErrOut.Log('Failed generator test.')
  else:
    InfoOut.Log('Passed generator test.')
  return total_errs


def Main(args):
  filenames = ParseOptions(args)
  return TestFiles(filenames)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

