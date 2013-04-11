#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Lexer for PPAPI IDL

The lexer uses the PLY library to build a tokenizer which understands both
WebIDL and Pepper tokens.

WebIDL, and WebIDL regular expressions can be found at:
   http://www.w3.org/TR/2012/CR-WebIDL-20120419/
PLY can be found at:
   http://www.dabeaz.com/ply/
"""

import optparse
import os.path
import sys

#
# Try to load the ply module, if not, then assume it is in the third_party
# directory.
#
try:
  # Disable lint check which fails to find the ply module.
  # pylint: disable=F0401
  from ply import lex
except:
  module_path, module_name = os.path.split(__file__)
  third_party = os.path.join(module_path, '..', '..', 'third_party')
  sys.path.append(third_party)
  # pylint: disable=F0401
  from ply import lex

#
# IDL Lexer
#
class IDLLexer(object):
  # 'tokens' is a value required by lex which specifies the complete list
  # of valid token types.
  tokens = [
    # Data types
      'float',
      'integer',
      'string',

    # Symbol and keywords types
      'COMMENT',
      'identifier',

    # MultiChar operators
      'ELLIPSIS',
  ]

  # 'keywords' is a map of string to token type.  All tokens matching
  # KEYWORD_OR_SYMBOL are matched against keywords dictionary, to determine
  # if the token is actually a keyword.
  keywords = {
    'any' : 'ANY',
    'attribute' : 'ATTRIBUTE',
    'boolean' : 'BOOLEAN',
    'byte' : 'BYTE',
    'callback' : 'CALLBACK',
    'const' : 'CONST',
    'creator' : 'CREATOR',
    'Date' : 'DATE',
    'deleter' : 'DELETER',
    'dictionary' : 'DICTIONARY',
    'DOMString' : 'DOMSTRING',
    'double' : 'DOUBLE',
    'enum'  : 'ENUM',
    'false' : 'FALSE',
    'float' : 'FLOAT',
    'exception' : 'EXCEPTION',
    'getter': 'GETTER',
    'implements' : 'IMPLEMENTS',
    'Infinity' : 'INFINITY',
    'inherit' : 'INHERIT',
    'interface' : 'INTERFACE',
    'legacycaller' : 'LEGACYCALLER',
    'long' : 'LONG',
    'Nan' : 'NAN',
    'null' : 'NULL',
    'object' : 'OBJECT',
    'octet' : 'OCTET',
    'optional' : 'OPTIONAL',
    'or' : 'OR',
    'partial'  : 'PARTIAL',
    'readonly' : 'READONLY',
    'sequence' : 'SEQUENCE',
    'setter': 'SETTER',
    'short' : 'SHORT',
    'static' : 'STATIC',
    'stringifier' : 'STRINGIFIER',
    'typedef' : 'TYPEDEF',
    'true' : 'TRUE',
    'unsigned' : 'UNSIGNED',
    'unrestricted' : 'UNRESTRICTED',
    'void' : 'VOID'
  }

  # Add keywords
  for key in keywords:
    tokens.append(keywords[key])

  # 'literals' is a value expected by lex which specifies a list of valid
  # literal tokens, meaning the token type and token value are identical.
  literals = '"*.(){}[],;:=+-/~|&^?<>'

  # Token definitions
  #
  # Lex assumes any value or function in the form of 't_<TYPE>' represents a
  # regular expression where a match will emit a token of type <TYPE>.  In the
  # case of a function, the function is called when a match is made. These
  # definitions come from WebIDL.

  # 't_ignore' is a special match of items to ignore
  t_ignore = ' \t'

  # Ellipsis operator
  t_ELLIPSIS = r'\.\.\.'

  # Constant values
  t_integer = r'-?(0([0-7]*|[Xx][0-9A-Fa-f]+)|[1-9][0-9]*)'
  t_float = r'-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)'
  t_float += r'([Ee][+-]?[0-9]+)?|[0-9]+[Ee][+-]?[0-9]+)'

  # A line ending '\n', we use this to increment the line number
  def t_LINE_END(self, t):
    r'\n+'
    self.AddLines(len(t.value))

  # We do not process escapes in the IDL strings.  Strings are exclusively
  # used for attributes and enums, and not used as typical 'C' constants.
  def t_string(self, t):
    r'"[^"]*"'
    t.value = t.value[1:-1]
    self.AddLines(t.value.count('\n'))
    return t

  # A C or C++ style comment:  /* xxx */ or //
  def t_COMMENT(self, t):
    r'(/\*(.|\n)*?\*/)|(//.*(\n[ \t]*//.*)*)'
    self.AddLines(t.value.count('\n'))
    return t

  # A symbol or keyword.
  def t_KEYWORD_OR_SYMBOL(self, t):
    r'_?[A-Za-z][A-Za-z_0-9]*'

    # All non-keywords are assumed to be symbols
    t.type = self.keywords.get(t.value, 'identifier')

    # We strip leading underscores so that you can specify symbols with the same
    # value as a keywords (E.g. a dictionary named 'interface').
    if t.value[0] == '_':
      t.value = t.value[1:]
    return t

  def t_ANY_error(self, t):
    msg = 'Unrecognized input'
    line = self.lexobj.lineno

    # If that line has not been accounted for, then we must have hit
    # EoF, so compute the beginning of the line that caused the problem.
    if line >= len(self.index):
      # Find the offset in the line of the first word causing the issue
      word = t.value.split()[0]
      offs = self.lines[line - 1].find(word)
      # Add the computed line's starting position
      self.index.append(self.lexobj.lexpos - offs)
      msg = 'Unexpected EoF reached after'

    pos = self.lexobj.lexpos - self.index[line]
    out = self.ErrorMessage(line, pos, msg)
    sys.stderr.write(out + '\n')
    self._lex_errors += 1


  def AddLines(self, count):
    # Set the lexer position for the beginning of the next line.  In the case
    # of multiple lines, tokens can not exist on any of the lines except the
    # last one, so the recorded value for previous lines are unused.  We still
    # fill the array however, to make sure the line count is correct.
    self.lexobj.lineno += count
    for _ in range(count):
      self.index.append(self.lexobj.lexpos)

  def FileLineMsg(self, line, msg):
    # Generate a message containing the file and line number of a token.
    filename = self.lexobj.filename
    if filename:
      return "%s(%d) : %s" % (filename, line + 1, msg)
    return "<BuiltIn> : %s" % msg

  def SourceLine(self, line, pos):
    # Create a source line marker
    caret = ' ' * pos + '^'
    # We decrement the line number since the array is 0 based while the
    # line numbers are 1 based.
    return "%s\n%s" % (self.lines[line - 1], caret)

  def ErrorMessage(self, line, pos, msg):
    return "\n%s\n%s" % (
        self.FileLineMsg(line, msg),
        self.SourceLine(line, pos))

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
    return tok


  def GetTokens(self):
    outlist = []
    while True:
      t = self.lexobj.token()
      if not t:
        break
      outlist.append(t)
    return outlist

  def Tokenize(self, data, filename='__no_file__'):
    self.lexobj.filename = filename
    self.lexobj.input(data)
    self.lines = data.split('\n')

  def __init__(self):
    self.index = [0]
    self._lex_errors = 0
    self.linex = []
    self.filename = None
    self.lexobj = lex.lex(object=self, lextab=None, optimize=0)

