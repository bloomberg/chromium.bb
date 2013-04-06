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

    # Operators
      'ELLIPSIS',
      'LSHIFT',
      'RSHIFT',

    # Symbol and keywords types
      'COMMENT',
      'identifier',

    # Pepper Extras
      'INLINE',
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
    'label' : 'LABEL',
    'legacycaller' : 'LEGACYCALLER',
    'long' : 'LONG',
    'namespace' : 'NAMESPACE',
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
    'struct' : 'STRUCT',
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

  # Constant values
  t_integer = r'-?(0([0-7]*|[Xx][0-9A-Fa-f]+)|[1-9][0-9]*)'
  t_float = r'-?(([0-9]+\.[0-9]*|[0-9]*\.[0-9]+)'
  t_float += r'([Ee][+-]?[0-9]+)?|[0-9]+[Ee][+-]?[0-9]+)'

  # Special multi-character operators
  t_ELLIPSIS = r'\.\.\.'
  t_LSHIFT = r'<<'
  t_RSHIFT = r'>>'

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

  # Return a "preprocessor" inline block
  def t_INLINE(self, t):
    r'\#inline (.|\n)*?\#endinl.*'
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
    self.lex_errors += 1


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
    caret = '\t^'.expandtabs(pos)
    # We decrement the line number since the array is 0 based while the
    # line numbers are 1 based.
    return "%s\n%s" % (self.lines[line - 1], caret)

  def ErrorMessage(self, line, pos, msg):
    return "\n%s\n%s" % (
        self.FileLineMsg(line, msg),
        self.SourceLine(line, pos))

  def GetTokens(self):
    outlist = []
    while True:
      t = self.lexobj.token()
      if not t:
        break
      outlist.append(t)
    return outlist

  def __init__(self, filename, data):
    self.index = [0]
    self.lex_errors = 0
    self.lines = data.split('\n')
    self.lexobj = lex.lex(object=self, lextab=None, optimize=0)
    self.lexobj.filename = filename
    self.lexobj.input(data)


#
# FileToTokens
#
# From a source file generate a list of tokens.
#
def FileToTokens(filename):
  with open(filename, 'rb') as srcfile:
    lexer = IDLLexer(filename, srcfile.read())
    return lexer.GetTokens()


#
# TextToTokens
#
# From a source file generate a list of tokens.
#
def TextToTokens(text):
  lexer = IDLLexer(None, text)
  return lexer.GetTokens()


#
# TestSameText
#
# From a set of tokens, generate a new source text by joining with a
# single space.  The new source is then tokenized and compared against the
# old set.
#
def TestSameText(filename):
  tokens1 = FileToTokens(filename)
  to_text = '\n'.join(['%s' % t.value for t in tokens1])
  tokens2 = TextToTokens(to_text)

  count1 = len(tokens1)
  count2 = len(tokens2)
  if count1 != count2:
    print "Size mismatch original %d vs %d\n" % (count1, count2)
    if count1 > count2:
      count1 = count2

  failed = 0
  for i in range(count1):
    if tokens1[i].value != tokens2[i].value:
      print "%d >>%s<< >>%s<<" % (i, tokens1[i].type, tokens2[i].value)
      failed = failed + 1

  return failed


#
# TestExpectedText
#
# From a set of tokens pairs, verify the type field of the second matches
# the value of the first, so that:
# integer 123 float 1.1
# will generate a passing test, where the first token has both the type and
# value of the keyword integer and the second has the type of integer and
# value of 123.
#
def TestExpect(filename):
  tokens = FileToTokens(filename)
  count = len(tokens)
  index = 0
  errors = 0
  while index < count:
    expect_type = tokens[index].value
    actual_type = tokens[index + 1].type
    index += 2

    if expect_type != actual_type:
      sys.stderr.write('Mismatch:  Expected %s, but got %s.\n' %
                       (expect_type, actual_type))
      errors += 1

  return errors


def Main(args):
  parser = optparse.OptionParser()
  parser.add_option('--test', help='Run tests.', action='store_true')

  # If no arguments are provided, run tests.
  if len(args) == 0:
    args = ['--test', 'test_lexer/values.in', 'test_lexer/keywords.in']

  options, filenames = parser.parse_args(args)
  if not filenames:
    parser.error('No files specified.')

  for filename in filenames:
    try:
      if options.test:
        if TestSameText(filename):
          sys.stderr.write('Failed text match on %s.\n' % filename)
          return -1
        if TestExpect(filename):
          sys.stderr.write('Failed expected type match on %s.\n' % filename)
          return -1
        print 'Passed: ' + filename

    except lex.LexError as le:
      sys.stderr.write('%s\n' % str(le))

  return 0


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
