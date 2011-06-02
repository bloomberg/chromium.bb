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
def GetArrayspec(node):
  assert(node.cls == 'Array')
  out = ''
  fixed = node.GetProperty('FIXED')
  if fixed:
    out = '[%s]' % fixed
  else:
    out = '[]'
  # Add children
  for child in node.GetListOf('Array'):
    out += GetArrayspec(child)
  return out


# Return the <name>[<size>] form of the node.
def GetNameArray(node, **keyargs):
  array = ''
  for child in node.GetListOf('Array'):
    array += GetArrayspec(child)
  return '%s%s' % (GetName(node, **keyargs), array)


# Get the 'return' type of the function.
def GetReturnType(node, **keyargs):
  root = node
  while root.typeinfo:
    root = root.typeinfo
  typeinfo = root.name
  typedata = TypeMap[typeinfo]
  return node.Replace(typedata['return'])


# Get the passing form of the parameter.
def GetParamType(node, **keyargs):
  root = node
  while root.typeinfo:
    root = root.typeinfo

  typeinfo = root.name
  typedata = TypeMap[typeinfo]
  if node.GetProperty('in'):
    return node.Replace(typedata['in'])
  if node.GetProperty('out'):
    return node.Replace(typedata['out'])
  return node.Replace(typedata['inout'])


# Return a list of arguments.
def GetArgList(node, **keyargs):
  assert(node.cls == 'Callspec')
  out = []
  for child in node.GetListOf('Param'):
    out.append(GetSignature(child, **keyargs))
  return out


# Return the signature of this node
def GetSignature(node, **keyargs):
  callnode = node.GetOneOf('Callspec')

  typeinfo  = node.GetProperty('TYPEREF')
  name = GetNameArray(node, **keyargs)
  if node.cls == 'Param':
    type = GetParamType(node, **keyargs)
  else:
    type = GetReturnType(node, **keyargs)

  if callnode:
    arglist = GetArgList(callnode, **keyargs)
    return '%s (*%s)(%s)' % (type, name, ', '.join(arglist))
  return '%s %s' % (type, name)


# Define an Enum.
def DefineEnum(node, **keyargs):
  out = "enum %s {\n" % GetName(node, **keyargs)

  enumlist = []
  for child in node.GetListOf('EnumItem'):
    value = child.GetProperty('VALUE')
    enumlist.append('  %s = %s' % (GetName(child), value))
  return "%s\n%s\n};\n" % (out, ',\n'.join(enumlist))

# Define an member Function.
def DefineFunction(node, **keyargs):
  return '%s;' % GetSignature(node, **keyargs)

# Define an Typedef.
def DefineTypedef(node, **keyargs):
  return "typedef %s;\n" % GetSignature(node, **keyargs)

# Define a Struct.
def DefineStruct(node, **keyargs):
  out = "struct %s {\n" % (GetName(node, **keyargs))
  for child in node.GetListOf('Function'):
    out += '  %s' % Define(child)
  for child in node.GetListOf('Member'):
    out += '  %s;\n' % GetSignature(child)
  out += "};"
  return out

# Define a C "prototype-able" object
def Define(node, **keyargs):
  declmap = {
    'Enum' : DefineEnum,
    'Function' : DefineFunction,
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
      ErrOut.Log("%s test failed with %d error(s)." % (filenode.name, errs))
      total_errs += errs

  if total_errs:
    ErrOut.Log("Failed generator test.")
  else:
    InfoOut.Log("Passed generator test.")
  return total_errs


def Main(args):
  filenames = ParseOptions(args)
  return TestFiles(filenames)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

