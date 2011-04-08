#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Lexer for PPAPI IDL """

#
# IDL Parser
#
# The parser is uses the PLY yacc library to build a set of parsing rules based
# on WebIDL.
#
# WebIDL, and WebIDL regular expressions can be found at:
#   http://dev.w3.org/2006/webapi/WebIDL/
# PLY can be found at:
#   http://www.dabeaz.com/ply/
#
# The parser generates a tree by recursively matching sets of items against
# defined patterns.  When a match is made, that set of items is reduced
# to a new item.   The new item can provide a match for parent patterns.
# In this way an AST is built (reduced) depth first.


import getopt
import os.path
import re
import sys

from idl_lexer import IDLLexer
from ply import lex
from ply import yacc

PARSER_OPTIONS = {
  'build_debug': False,
  'parse_debug': False,
  'token_debug': False,
  'output': False,
  'verbose': False
}

#
# ERROR_REMAP
#
# Maps the standard error formula into a more friendly error message.
#
ERROR_REMAP = {
  'Unexpected ")" after "(".' : 'Empty argument list.',
  'Unexpected ")" after ",".' : 'Missing argument.',
  'Unexpected "}" after ",".' : 'Trailing comma in block.',
  'Unexpected "}" after "{".' : 'Unexpected empty block.',
  'Unexpected comment "/*" after "}".' : 'Unexpected trailing comment.',
  'Unexpected "{" after keyword "enum".' : 'Enum missing name.',
  'Unexpected "{" after keyword "struct".' : 'Struct missing name.',
  'Unexpected "{" after keyword "interface".' : 'Interface missing name.',
}

# DumpReduction
#
# Prints out the set of items which matched a particular pattern and the
# new item or set it was reduced to.
def DumpReduction(cls, p):
  if p[0] is None:
    print "OBJ: %s(%d) - None" % (cls, len(p))
  else:
    out = ""
    for index in range(len(p) - 1):
      out += " >%s< " % str(p[index + 1])
    print "OBJ: %s(%d) - %s : %s"  % (cls, len(p), str(p[0]), out)


# CopyToList
#
# Takes an input item, list, or None, and returns a new list of that set.
def CopyToList(item):
  # If the item is 'Empty' make it an empty list
  if not item: item = []

  # If the item is not a list
  if type(item) is not type([]): item = [item]

  # Make a copy we can modify
  return list(item)



# ListFromConcat
#
# Generate a new List by joining of two sets of inputs which can be an
# individual item, a list of items, or None.
def ListFromConcat(*items):
  itemsout = []
  for item in items:
    itemlist = CopyToList(item)
    itemsout.extend(itemlist)

  return itemsout


# TokenTypeName
#
# Generate a string which has the type and value of the token.
def TokenTypeName(t):
  if t.type == 'SYMBOL':  return 'symbol %s' % t.value
  if t.type in ['HEX', 'INT', 'OCT', 'FLOAT']:
    return 'value %s' % t.value
  if t.type == 'STRING' : return 'string "%s"' % t.value
  if t.type == 'COMMENT' : return 'comment "%s"' % t.value[:2]
  if t.type == t.value: return '"%s"' % t.value
  return 'keyword "%s"' % t.value


# Send a string to stdout
def PrintInfo(text):
  sys.stdout.write("%s\n" % text)

def PrintError(text):
  sys.stderr.write("%s\n" % text)

# Send a string to stderr containing a file, line number and error message
def LogError(filename, lineno, pos, msg):
  PrintError("%s(%d) : %s\n" % (filename, lineno + 1, msg))


#
# IDL Parser
#
# The Parser inherits the from the Lexer to provide PLY with the tokenizing
# definitions.  Parsing patterns are encoded as function where p_<name> is
# is called any time a patern matching the function documentation is found.
# Paterns are expressed in the form of:
# """ <new item> : <item> ....
#                | <item> ...."""
#
# Where new item is the result of a match against one or more sets of items
# separated by the "|".
#
# The function is called with an object 'p' where p[0] is the output object
# and p[n] is the set of inputs for positive values of 'n'.  Len(p) can be
# used to distinguish between multiple item sets in the pattern.
#
# For more details on parsing refer to the PLY documentation at
#    http://www.dabeaz.com/ply/
#
#
# The parser uses the following conventions:
#   a <type>_block defines a block of <type> definitions in the form of:
#       [comment] [ext_attr_block] <type> <name> '{' <type>_list '}' ';'
#   A block is reduced by returning an object of <type> with a name of <name>
#   which in turn has <type>_list as children.
#
#   A [comment] is a optional C style comment block enclosed in /* ... */ which
#   is appended to the adjacent node as a child.
#
#   A [ext_attr_block] is an optional list of Extended Attributes which is
#   appended to the adjacent node as a child.
#
#   a <type>_list defines a list of <type> items which will be passed as a
#   list of children to the parent pattern.  A list is in the form of:
#       [comment] [ext_attr_block] <...DEF...> ';' <type>_list | (empty)
# or
#       [comment] [ext_attr_block] <...DEF...> <type>_cont
#
#   In the first form, the list is reduced recursively, where the right side
#   <type>_list is first reduced then joined with pattern currently being
#   matched.  The list is terminated with the (empty) pattern is matched.
#
#   In the second form the list is reduced recursively, where the right side
#   <type>_cont is first reduced then joined with the pattern currently being
#   matched.  The type_<cont> is in the form of:
#       ',' <type>_list | (empty)
#   The <type>_cont form is used to consume the ',' which only occurs when
#   there is more than one object in the list.  The <type>_cont also provides
#   the terminating (empty) definition.
#


class IDLParser(IDLLexer):
# TOP
#
# This pattern defines the top of the parse tree.  The parse tree is in the
# the form of:
#
# top
#   *modifiers
#     *comments
#     *ext_attr_block
#       ext_attr_list
#          attr_arg_list
#   *integer, value
#   *param_list
#   *typeref
#
#   top_list
#     describe_block
#       describe_list
#     enum_block
#       enum_type
#     interface_block
#     struct_block
#       member
#     typedef_decl
#       typedef_data
#       typedef_func
#
# (* sub matches found at multiple levels and are not truly children of top)
#
# We force all input files to start with two comments.  The first comment is a
# Copyright notice followed by a set of file wide Extended Attributes, followed
# by the file comment and finally by file level patterns.
#
  # Find the Copyright, File comment, and optional file wide attributes.  We
  # use a match with COMMENT instead of comments to force the token to be
  # present.  The extended attributes and the top_list become siblings which
  # in turn are children of the file object created from the results of top.
  def p_top(self, p):
    """top : COMMENT COMMENT ext_attr_block top_list"""

    Copyright = self.BuildProduction('Copyright', p, 1, None)
    Filedoc = self.BuildProduction('Comment', p, 2, None)

    out = ListFromConcat(p[3], p[4])
    out = ListFromConcat(Filedoc, out)
    p[0] = ListFromConcat(Copyright, out)
    if self.parse_debug: DumpReduction('top', p)

  # Build a list of top level items.
  def p_top_list(self, p):
    """top_list : describe_block top_list
                | enum_block top_list
                | interface_block top_list
                | struct_block top_list
                | typedef_def top_list
                | """
    if len(p) > 2:
      p[0] = ListFromConcat(p[1], p[2])
    if self.parse_debug: DumpReduction('top_list', p)

  # Recover from error and continue parsing at the next top match.
  def p_top_error(self, p):
    """top_list : error top_list"""
    p[0] = p[2]

#
# Modifier List
#
#
  def p_modifiers(self, p):
    """modifiers : comments ext_attr_block"""
    p[0] = ListFromConcat(p[1], p[2])
    if self.parse_debug: DumpReduction('modifiers', p)

#
# Comments
#
# Comments are optional list of C style comment objects.  Comments are returned
# as a list or None.
#
  def p_comments(self, p):
    """comments : COMMENT comments
                | """
    if len(p) > 1:
      child = self.BuildProduction('Comment', p, 1, None)
      p[0] = ListFromConcat(child, p[2])
      if self.parse_debug: DumpReduction('comments', p)
    else:
      if self.parse_debug: DumpReduction('no comments', p)

#
# Extended Attributes
#
# Extended Attributes denote properties which will be applied to a node in the
# AST.  A list of extended attributes are denoted by a brackets '[' ... ']'
# enclosing a comma separated list of extended attributes in the form of:
#
#  Name
#  Name=HEX | INT | OCT | FLOAT
#  Name="STRING"
#  Name=Function(arg ...)
#  TODO(noelallen) -Not currently supported:
#  ** Name(arg ...) ...
#  ** Name=Scope::Value
#
# Extended Attributes are returned as a list or None.

  def p_ext_attr_block(self, p):
    """ext_attr_block : '[' ext_attr_list ']'
                  | """
    if len(p) > 1:
      p[0] = p[2]
      if self.parse_debug: DumpReduction('ext_attr_block', p)
    else:
      if self.parse_debug: DumpReduction('no ext_attr_block', p)

  def p_ext_attr_list(self, p):
    """ext_attr_list : SYMBOL '=' value ext_attr_cont
                     | SYMBOL '(' attr_arg_list ')' ext_attr_cont
                     | SYMBOL ext_attr_cont """
    if len(p) == 3:
      p[0] = ListFromConcat(self.BuildExtAttribute(p[1], 'True'), p[2])
    if len(p) == 5:
      p[0] = ListFromConcat(self.BuildExtAttribute(p[1], p[3]), p[4])
    if len(p) == 6:
      p[0] = ListFromConcat(self.BuildExtAttribute(p[1], p[3]), p[5])
    if self.parse_debug: DumpReduction('ext_attribute_list', p)

  def p_ext_attr_cont(self, p):
    """ext_attr_cont : ',' ext_attr_list
                     |"""
    if len(p) > 1:
      p[0] = ListFromConcat(p[2], p[3])
    if self.parse_debug: DumpReduction('ext_attribute_cont', p)

  def p_attr_arg_list(self, p):
    """attr_arg_list : SYMBOL attr_arg_cont
                     | value attr_arg_cont """
    p[0] = ','.join(ListFromConcat(p[1], p[2]))
    if self.parse_debug: DumpReduction('attr_arg_list', p)

  def p_attr_arg_cont(self, p):
    """attr_arg_cont : ',' attr_arg_list
                     | """
    if len(p) > 1: p[0] = p[2]
    if self.parse_debug: DumpReduction('attr_arg_cont', p)

  def p_attr_arg_error(self, p):
    """attr_arg_cont : error attr_arg_cont"""
    p[0] = p[2]
    if self.parse_debug: DumpReduction('attr_arg_error', p)


#
# Describe
#
# A describe block is defined at the top level.  It provides a mechanism for
# attributing a group of ext_attr to a describe_list.  Members of the
# describe list are language specific 'Type' declarations
#
  def p_describe_block(self, p):
    """describe_block : modifiers DESCRIBE '{' describe_list '}' ';'"""
    children = ListFromConcat(p[1], p[2])
    p[0] = self.BuildProduction('Describe', p, 2, children)
    if self.parse_debug: DumpReduction('describe_block', p)

  def p_describe_list(self, p):
    """describe_list : modifiers SYMBOL ';' describe_list
                     | modifiers ENUM ';' describe_list
                     | modifiers STRUCT ';' describe_list
                     | modifiers TYPEDEF ';' describe_list
                     | """
    if len(p) > 1:
      Type = self.BuildProduction('Type', p, 2, p[1])
      p[0] = ListFromConcat(Type, p[4])

  def p_describe_error(self, p):
    """describe_list : error describe_list"""
    p[0] = p[2]

#
# Constant Values (integer, value)
#
# Constant values can be found at various levels.  A Constant value is returns
# as the string value after validated against a FLOAT, HEX, INT, OCT or
# STRING pattern as appropriate.
#
  def p_value(self, p):
    """value : FLOAT
             | HEX
             | INT
             | OCT
             | STRING"""
    p[0] = p[1]
    if self.parse_debug: DumpReduction('value', p)

  def p_value_lshift(self, p):
    """value : integer LSHIFT INT"""
    p[0] = "(%s << %s)" % (p[1], p[3])
    if self.parse_debug: DumpReduction('value', p)

# Integers are numbers which may not be floats used in cases like array sizes.
  def p_integer(self, p):
    """integer : HEX
               | INT
               | OCT"""
    p[0] = p[1]

#
# Parameter List
#
# A parameter list is a collection of arguments which are passed to a
# function.  In the case of a PPAPI, it is illegal to have a function
# which passes no parameters.
#
# NOTE:-We currently do not support functions which take no arguments in PPAPI.
  def p_param_list(self, p):
    """param_list : modifiers typeref SYMBOL param_cont"""
    children = ListFromConcat(p[1], p[2])
    param = self.BuildProduction('Param', p, 3, children)
    p[0] = ListFromConcat(param, p[4])
    if self.parse_debug: DumpReduction('param_list', p)

  def p_param_cont(self, p):
    """param_cont : ',' param_list
                  | """
    if len(p) > 1:
      p[0] = p[2]
      if self.parse_debug: DumpReduction('param_cont', p)

  def p_param_error(self, p):
    """param_cont : error param_cont"""
    p[0] = p[2]

#
# Typeref
#
# A typeref is a reference to a type definition.  The type definition may
# be a built in such as int32_t or a defined type such as an enum, or
# struct, or typedef.  Part of the reference to the type is how it is
# used, such as directly, a fixed size array, or unsized (pointer).  The
# reference is reduced and becomes a property of the parent Node.
#
  def p_typeref_data(self, p):
    """typeref : SYMBOL typeref_arrays"""

    Type = self.BuildExtAttribute('TYPEREF', p[1])
    p[0] = ListFromConcat(Type, p[2])
    if self.parse_debug: DumpReduction('typeref', p)

  def p_typeref_arrays(self, p):
    """typeref_arrays : '[' ']' typeref_arrays
                           | '[' integer ']' typeref_arrays
                           | """
    if len(p) == 1: return
    if len(p) == 5:
      count = self.BuildExtAttribute('FIXED', p[2])
      array = self.BuildProduction('Array', p, 2, ListFromConcat(p[4], count))
    else:
      array = self.BuildProduction('Array', p, 1, p[3])

    p[0] = [array]
    if self.parse_debug: DumpReduction('arrays', p)

#
# Enumeration
#
# An enumeration is a set of named integer constants.  An enumeration
# is valid type which can be referenced in other definitions.
#
  def p_enum_block(self, p):
    """enum_block : modifiers ENUM SYMBOL '{' enum_list '}' ';'"""
    p[0] = self.BuildProduction('Enum', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('enum_block', p)

  def p_enum_list(self, p):
    """enum_list : comments SYMBOL '=' value enum_cont"""
    val  = self.BuildExtAttribute('VALUE', p[4])
    enum = self.BuildProduction('EnumItem', p, 2, ListFromConcat(val, p[1]))
    p[0] = ListFromConcat(enum, p[5])
    if self.parse_debug: DumpReduction('enum_list', p)

  def p_enum_cont(self, p):
    """enum_cont : ',' enum_list
                 |"""
    if len(p) > 1: p[0] = p[2]
    if self.parse_debug: DumpReduction('enum_cont', p)

  def p_enum_cont_error(self, p):
    """enum_cont : error enum_cont"""
    p[0] = p[2]
    if self.parse_debug: DumpReduction('enum_error', p)


#
# Interface
#
# An interface is a named collection of functions.
#
  def p_interface_block(self, p):
    """interface_block : modifiers INTERFACE SYMBOL '{' member_list '}' ';'"""
    p[0] = self.BuildProduction('Interface', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('interface_block', p)

  def p_member_list(self, p):
    """member_list : member_function member_list
                   | """
    if len(p) > 1 :
      p[0] = ListFromConcat(p[1], p[2])
      if self.parse_debug: DumpReduction('member_list', p)

  def p_member_function(self, p):
    """member_function : modifiers typeref SYMBOL '(' param_list ')' ';'"""
    params = self.BuildProduction('Callspec', p, 4, p[5])
    p[0] = self.BuildProduction('Function', p, 3, ListFromConcat(p[1], params))
    if self.parse_debug: DumpReduction('member_function', p)

  def p_member_error(self, p):
    """member_list : error member_list"""
    p[0] = p[2]

#
# Struct
#
# A struct is a named collection of members which in turn reference other
# types.  The struct is a referencable type.
#
  def p_struct_block(self, p):
    """struct_block : modifiers STRUCT SYMBOL '{' struct_list '}' ';'"""
    p[0] = self.BuildProduction('Struct', p, 3, ListFromConcat(p[1], p[5]))
    if self.parse_debug: DumpReduction('struct_block', p)

  def p_struct_list(self, p):
    """struct_list : modifiers typeref SYMBOL ';' struct_list
                   | """
    if len(p) > 1:
      member = self.BuildProduction('Member', p, 3, ListFromConcat(p[1], p[2]))
      p[0] = ListFromConcat(member, p[5])
    if self.parse_debug: DumpReduction('struct_list', p)

#
# Typedef
#
# A typedef creates a new referencable type.  The tyepdef can specify an array
# definition as well as a function declaration.
#
  def p_typedef_data(self, p):
    """typedef_def : modifiers TYPEDEF typeref SYMBOL ';' """
    p[0] = self.BuildProduction('Typedef', p, 4, ListFromConcat(p[1], p[3]))
    if self.parse_debug: DumpReduction('typedef_data', p)

  def p_typedef_func(self, p):
    """typedef_def : modifiers TYPEDEF typeref SYMBOL '(' param_list ')' ';'"""
    params = self.BuildProduction('Callspec', p, 5, p[6])
    children = ListFromConcat(p[1], p[3], params)
    p[0] = self.BuildProduction('Typedef', p, 4, children)
    if self.parse_debug: DumpReduction('typedef_func', p)


#
# Parser Errors
#
# p_error is called whenever the parser can not find a pattern match for
# a set of items from the current state.  The p_error function defined here
# is triggered logging an error, and parsing recover happens as the
# p_<type>_error functions defined above are called.  This allows the parser
# to continue so as to capture more than one error per file.
#
  def p_error(self, t):
    filename = self.lexobj.filename
    self.parse_errors += 1
    if t:
      lineno = t.lineno
      pos = t.lexpos
      prev = self.yaccobj.symstack[-1]
      if type(prev) == lex.LexToken:
        msg = "Unexpected %s after %s." % (
            TokenTypeName(t), TokenTypeName(prev))
      else:
        msg = "Unexpected %s." % (t.value)
    else:
      lineno = self.last.lineno
      pos = self.last.lexpos
      msg = "Unexpected end of file after %s." % TokenTypeName(self.last)
      self.yaccobj.restart()

    # Attempt to remap the error to a friendlier form
    if msg in ERROR_REMAP:
      msg = ERROR_REMAP[msg]

    # Log the error
    self.Logger(filename, lineno, pos, msg)


  def __init__(self, builder, logger, options = {}):
    global PARSER_OPTIONS

    IDLLexer.__init__(self, options)
    self.yaccobj = yacc.yacc(module=self, tabmodule=None, debug=False,
                             optimize=0, write_tables=0)

    for k in options:
      PARSER_OPTIONS[k] = options[k]

    self.build_debug = PARSER_OPTIONS['build_debug']
    self.parse_debug = PARSER_OPTIONS['parse_debug']
    self.token_debug = PARSER_OPTIONS['token_debug']
    self.verbose = PARSER_OPTIONS['verbose']
    self.Builder = builder
    self.Logger = logger
    self.parse_errors = 0

#
# Tokenizer
#
# The token function returns the next token provided by IDLLexer for matching
# against the leaf paterns.
#
  def token(self):
    tok = self.lexobj.token()
    if tok:
      self.last = tok
      if self.token_debug:
        PrintInfo("TOKEN %s(%s)" % (tok.type, tok.value))
    return tok

#
# BuildProduction
#
# Production is the set of items sent to a grammar rule resulting in a new
# item being returned.
#
# p - Is the Yacc production object containing the stack of items
# index - Index into the production of the name for the item being produced.
# cls - The type of item being producted
# childlist - The children of the new item
  def BuildProduction(self, cls, p, index, childlist):
    name = p[index]
    filename = self.lexobj.filename
    lineno = p.lineno(index)
    pos = p.lexpos(index)
    if self.build_debug:
      PrintInfo("Building %s(%s)" % (cls, name))
    return self.Builder(cls, name, filename, lineno, pos, childlist)

#
# BuildExtAttribute
#
# An ExtendedAttribute is a special production that results in a property
# which is applied to the adjacent item.  Attributes have no children and
# instead represent key/value pairs.
#
  def BuildExtAttribute(self, name, value):
    if self.build_debug:
      PrintInfo("Adding ExtAttribute %s = %s" % (name, str(value)))
    return self.Builder('ExtAttribute', '%s=%s' % (name,value),
        self.lexobj.filename, self.last.lineno, self.last.lexpos, [])

#
# ParseData
#
# Attempts to parse the current data loaded in the lexer.
#
  def ParseData(self):
    try:
      return self.yaccobj.parse(lexer=self)

    except lex.LexError as le:
      PrintError(str(le))
      return []

#
# ParseFile
#
# Loads a new file into the lexer and attemps to parse it.
#
  def ParseFile(self, filename):
    data = open(filename).read()
    self.SetData(filename, data)
    if self.verbose:
      PrintInfo("Parsing %s" % filename)
    try:
      out = self.ParseData()
      return out

    except Exception as e:
      LogError(filename, self.last.lineno, self.last.lexpos,
               'Internal parsing error - %s.' % str(e))
      raise
      return []




class TestNode(object):
  def __init__(self, cls, name, filename, lineno, pos, children):
    self.cls = cls
    self.name = name
    if children:
      self.childlist = children
    else:
      self.childlist = []

  def __str__(self):
    return "%s(%s)" % (self.cls, self.name)

  def Dump(self, depth, comments = False, out=sys.stdout):
    if not comments:
      if self.cls == 'Comment' or self.cls == 'Copyright':
        return

    tab = ""
    for t in range(depth):
      tab += '  '

    print >>out, "%s%s" % (tab, self)
    for c in self.childlist:
      c.Dump(depth + 1, out)

def FlattenTree(node):
  add_self = False
  out = []
  for c in node.childlist:
    if c.cls == 'Comment':
      add_self = True
    else:
      out.extend(FlattenTree(c))

  if add_self:
    out = [str(node)] + out
  return out


err_list = []
def TestLog(filename, lineno, pos, msg):
  global err_list
  global PARSER_OPTIONS

  err_list.append(msg)
  if PARSER_OPTIONS['verbose']:
    sys.stdout.write("%s(%d) : %s\n" % (filename, lineno + 1, msg))


def Test(filename, nodes):
  global err_list
  lexer = IDLLexer()
  data = open(filename).read()
  lexer.SetData(filename, data)

  pass_comments = []
  fail_comments = []
  while True:
    tok = lexer.lexobj.token()
    if tok == None: break
    if tok.type == 'COMMENT':
      args = tok.value.split()
      if args[1] == 'OK':
        pass_comments.append((tok.lineno, ' '.join(args[2:-1])))
      else:
        if args[1] == 'FAIL':
          fail_comments.append((tok.lineno, ' '.join(args[2:-1])))
  obj_list = []
  for node in nodes:
    obj_list.extend(FlattenTree(node))

  errors = 0

  #
  # Check for expected successes
  #
  obj_cnt = len(obj_list)
  pass_cnt = len(pass_comments)
  if obj_cnt != pass_cnt:
    PrintInfo("Mismatched pass (%d) vs. nodes built (%d)."
        % (pass_cnt, obj_cnt))
    PrintInfo("PASS: %s" % [x[1] for x in pass_comments])
    PrintInfo("OBJS: %s" % obj_list)
    errors += 1
    if pass_cnt > obj_cnt: pass_cnt = obj_cnt

  for i in range(pass_cnt):
    line, comment = pass_comments[i]
    if obj_list[i] != comment:
      print "%s(%d) Error: OBJ %s : EXPECTED %s" % (
        filename, line, obj_list[i], comment)
      errors += 1

  #
  # Check for expected errors
  #
  err_cnt = len(err_list)
  fail_cnt = len(fail_comments)
  if err_cnt != fail_cnt:
    PrintInfo("Mismatched fail (%d) vs. errors seen (%d)."
        % (fail_cnt, err_cnt))
    PrintInfo("FAIL: %s" % [x[1] for x in fail_comments])
    PrintInfo("ERRS: %s" % err_list)
    errors += 1
    if fail_cnt > err_cnt:  fail_cnt = err_cnt

  for i in range(fail_cnt):
    line, comment = fail_comments[i]
    if err_list[i] != comment:
      PrintError("%s(%d) Error\n\tERROR : %s\n\tEXPECT: %s" % (
        filename, line, err_list[i], comment))
      errors += 1

  # Clear the error list for the next run
  err_list = []
  return errors


def Main(args):
  global PARSER_OPTIONS

  long_opts = PARSER_OPTIONS.keys()
  usage = 'Usage: idl_parser.py %s [<src.idl> ...]' % ' '.join(long_opts)
  try:
    opts, filenames = getopt.getopt(args, '', long_opts)

  except getopt.error, e:
    PrintError('Illegal option: %s\n\t%s' % (str(e) % usage))
    return 1

  for opt, val in opts:
    PARSER_OPTIONS[opt[2:]] = True
    print 'Set %s to True.' % opt

  parser = IDLParser(TestNode, TestLog, PARSER_OPTIONS)

  total_errs = 0
  for filename in filenames:
    tokens = parser.ParseFile(filename)
    errs = Test(filename, tokens)
    total_errs += errs
    if errs:
      PrintError("%s test failed with %d error(s)." % (filename, errs))
    else:
      PrintInfo("%s passed." % filename)

    if PARSER_OPTIONS['output']:
      for token in tokens:
         token.Dump(0)

  return total_errs


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

