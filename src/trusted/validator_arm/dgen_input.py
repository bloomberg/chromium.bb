#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
A simple recursive-descent parser for the table file format.

The grammar implemented here is roughly (taking some liberties with whitespace
and comment parsing):

table_file ::= classdef* table+ eof ;

action         ::= decoder_action arch | decoder_method | '"'
arch           ::= '(' word+ ')'
citation       ::= '(' word+ ')'
classdef       ::= 'class' word ':' word
decoder        ::= id ('=>' id)?
decoder_action ::= '=' decoder (id (word (id)?)?)?
decoder_method ::= '->' id
default_row    ::= 'else' ':' action
footer         ::= '+' '-' '-'
header         ::= "|" (id '(' int (':' int)? ')')+
int            ::= word     (where word is a sequence of digits)
id             ::= word     (where word is sequence of letters, digits and _)
parenthesized_exp ::= '(' (word | punctuation)+ ')'
pat_row        ::= pattern+ action
pattern        ::= 'word' | '-' | '"'
row            ::= '|' (pat_row | default_row)
table          ::= table_desc header row+ footer
table_desc     ::= '+' '-' '-' id citation?

If a decoder_action has more than one element, the interpretation is as follows:
   id[0] = action (plus optional architecture) to apply.
   id[1] = Arm rule action corresponds to.
   word = Bit pattern of rule.
   id[3] = Name defining additional constraints for match.
"""

import re
import dgen_core

def parse_tables(input):
  """Entry point for the parser.  Input should be a file or file-like."""
  parser = Parser()
  return parser.parse(input)

class Token(object):
  """Holds a (characterized) unit of text for the parser."""

  def __init__(self, kind, value=None):
    self.kind = kind
    self.value = value if value else kind

class Parser(object):
  """Parses a set of tables from the input file."""

  def parse(self, input):
    self.input = input             # The remaining input to parse
    decoder = dgen_core.Decoder()  # The generated decoder of parse tables.
    # Read the class definitions while there.
    while self._next_token().kind == 'class':
      self._classdef(decoder)

    # Read tables while there.
    while self._next_token().kind == '+':
      self._table(decoder)

    if not self._next_token().kind == 'eof':
      self._unexpected('unrecognized input found')
    if not decoder.primary:
      self._unexpected('No primary table defined')
    if not decoder.tables():
      self._unexpected('No tables defined')
    return decoder

  def __init__(self):
    self._words = []             # Words left on current line, not yet parsed.
    self._line_no = 0            # The current line being parsed
    self._token = None           # The next token from the input.
    self._reached_eof = False    # True when end of file reached
    # Punctuation and keywords allowed. Must be ordered such that if
    # p1 != p2 are in the list, and p1.startswith(p2), then
    # p1 must appear before p2.
    self._punctuation = ['class', 'else', '=>', '->', '-', '+', '(', ')',
                         '=', ':', '"', '|']

  def _action(self, last_action, last_arch):
    """ action ::= decoder_action arch | decoder_method | '"' """
    if self._next_token().kind == '"':
      self._read_token('"')
      return (last_action, last_arch)
    if self._next_token().kind == '=':
      action = self._decoder_action()
      arch = None
      if self._next_token().kind == '(':
        arch = self._arch()
      return (action, arch)
    elif self._next_token().kind == '->':
      return (self._decoder_method(), None)
    else:
      self._unexpected("Row doesn't define an action")

  def _arch(self):
    """ arch ::= '(' word+ ')' """
    return ' '.join(self._parenthesized_exp())

  def _citation(self):
    """ citation ::= '(' word+ ')' """
    return ' '.join(self._parenthesized_exp())

  def _classdef(self, decoder):
    """ classdef       ::= 'class' word ':' word """
    self._read_token('class')
    class_name = self._read_token('word').value
    self._read_token(':');
    class_superclass = self._read_token('word').value
    if not decoder.add_class_def(class_name, class_superclass):
      unexpected('Inconsistent definitions for class %s' % class_name)

  def _read_id_or_none(self, read_id):
    if self._next_token().kind in ['|', '+', '(']:
      return None
    id = self._id() if read_id else self._read_token('word').value
    return None if id and id == 'None' else id

  def _decoder(self):
    """ decoder        ::= id ('=>' id)? """
    baseline = self._id()
    actual = baseline
    if self._next_token().kind == '=>':
      self._read_token('=>')
      actual = self._id()
    return (baseline, actual)

  def _decoder_action(self):
    """ decoder_action ::= '=' decoder (id (word (id)?)?)? """
    self._read_token('=')
    (baseline, actual) = self._decoder()
    rule = self._read_id_or_none(True)
    pattern = self._read_id_or_none(False)
    constraints = self._read_id_or_none(True)
    return dgen_core.DecoderAction(baseline, actual, rule, pattern, constraints)

  def _decoder_method(self):
    """ decoder_method ::= '->' id """
    self._read_token('->')
    name = self._id()
    return dgen_core.DecoderMethod(name)

  def _default_row(self, table, last_action, last_arch):
    """ default_row ::= 'else' ':' action """
    self._read_token('else')
    self._read_token(':')
    (action, arch) = self._action(last_action, last_arch)
    if not table.add_default_row(action, arch):
      self._unexpected('Unable to install row default')
    return (None, action, arch)

  def _footer(self):
    """ footer ::= '+' '-' '-' """
    self._read_token('+')
    self._read_token('-')
    self._read_token('-')

  def _header(self, table):
    """ header ::= "|" (id '(' int (':' int)? ')')+ """
    self._read_token('|')
    while not self._next_token().kind == '|':
      name = self._read_token('word').value
      self._read_token('(')
      hi_bit = self._int()
      lo_bit = hi_bit
      if self._next_token().kind == ':':
        self._read_token(':')
        lo_bit = self._int()
      self._read_token(')')
      table.add_column(name, hi_bit, lo_bit)

  def _int(self):
    """ int ::= word

    Int is a sequence of digits. Returns the corresponding integer.
    """
    word = self._read_token('word').value
    m = re.match(r'^([0-9]+)$', word)
    if m:
      return int(word)
    else:
      self._unexpected('integer expected but found "%s"' % word)

  def _id(self):
    """ id ::= word

    Word starts with a letter, and followed by letters, digits,
    and underscores. Returns the corresponding identifier.
    """
    ident = self._read_token('word').value
    m = re.match(r'^[a-zA-z][a-zA-z0-9_]*$', ident)
    if not m:
      self._unexpected('"%s" is not a valid identifier' % ident)
    return ident

  def _parenthesized_exp(self, minlength=1):
    """ parenthesized_exp ::= '(' (word | punctuation)+ ')'

    The punctuation doesn't include ')'.
    Returns the sequence of token values parsed.
    """
    self._read_token('(')
    words = []
    while not self._at_eof() and self._next_token().kind != ')':
      words.append(self._read_token().value)
    if len(words) < minlength:
      self._unexpected("len(parenthesized expresssion) < %s" % minlength)
    self._read_token(')')
    return words
  def _pat_row(self, table, last_patterns, last_action, last_arch):
    """ pat_row ::= pattern+ action

    Passed in sequence of patterns and action from last row,
    and returns list of patterns and action from this row.
    """
    patterns = []             # Patterns as found on input.
    expanded_patterns = []    # Patterns after being expanded.
    num_patterns = 0
    num_patterns_last = len(last_patterns) if last_patterns else None
    while self._next_token().kind not in ['=', '->', '|', '+']:
      if not last_patterns or num_patterns < num_patterns_last:
        last_pattern = last_patterns[num_patterns] if last_patterns else None
        pattern = self._pattern(last_pattern)
        patterns.append(pattern)
        expanded_patterns.append(table.define_pattern(pattern, num_patterns))
        num_patterns += 1
      else:
        # Processed patterns in this row, since width is now the
        # same as last row.
        break;

    (action, arch) = self._action(last_action, last_arch)
    table.add_row(expanded_patterns, action, arch)
    return (patterns, action, arch)

  def _pattern(self, last_pattern):
    """ pattern ::= 'word' | '-' | '"'

    Arguments are:
      col_no - The current column entry being read.
      last_patterns - The list of patterns defined on the last row.
      last_action - The action defined on the last row.
      last_arch - The architecture defined on the last row..
    """
    if self._next_token().kind == '"':
      self._read_token('"')
      return last_pattern
    if self._next_token().kind in ['-', 'word']:
      return self._read_token().value
    self._unexpected('Malformed pattern')

  def _row(self, table, last_patterns=None,
           last_action=None, last_arch= None):
    """ row ::= '|' (pat_row | default_row)

    Passed in sequence of patterns and action from last row,
    and returns list of patterns and action from this row.
    """
    self._read_token('|')
    if self._next_token().kind == 'else':
      return self._default_row(table, last_action, last_arch)
    else:
      return self._pat_row(table, last_patterns, last_action, last_arch)

  def _table(self, decoder):
    """ table ::= table_desc header row+ footer """
    table = self._table_desc()
    print 'Reading table %s...' % table.name
    self._header(table)
    (pattern, action, arch) = self._row(table)
    while not self._next_token().kind == '+':
      (pattern, action, arch) = self._row(table, pattern, action, arch)
    if not decoder.add(table):
      self._unexpected('Multiple tables with name %s' % table.name)
    self._footer()

  def _table_desc(self):
    """ table_desc ::= '+' '-' '-' id citation? """
    self._read_token('+')
    self._read_token('-')
    self._read_token('-')
    name = self._id()
    citation = None
    if self._next_token().kind == '(':
      citation = self._citation()
    return dgen_core.Table(name, citation)

  def _at_eof(self):
    """Returns true if next token is the eof token."""
    return self._next_token().kind == 'eof'

  def _read_token(self, kind=None):
    """Reads and returns the next token from input."""
    token = self._next_token()
    self._token = None
    if kind and kind != token.kind:
      self._unexpected('Expected "%s" but found "%s"'
                       % (kind, token.kind))
    return token

  def _next_token(self):
    """Returns the next token from the input."""
    # First seee if cached.
    if self._token: return self._token

    # If no more tokens left on the current line. read
    # input till more tokens are found
    while not self._reached_eof and not self._words:
      self._words = self._read_line().split()

    if self._words:
      # More tokens found. Convert the first word to a token.
      word = self._words.pop(0)
      # First remove any applicable punctuation.
      for p in self._punctuation:
        index = word.find(p)
        if index == 0:
          # Found punctuation, return it.
          self._pushback(word[len(p):])
          self._token = Token(p)
          return self._token
        elif index > 0:
          self._pushback(word[index:])
          word = word[:index]
      # if reached, word doesn't contain any punctuation, so return it.
      self._token = Token('word', word)
    else:
      # No more tokens found, assume eof.
      self._token = Token('eof')
    return self._token

  def _pushback(self, word):
    """Puts word back onto the list of words."""
    if word:
      self._words.insert(0, word)

  def _read_line(self):
    """Reads the next line of input, and returns it. Otherwise None."""
    self._line_no += 1
    line = self.input.readline()
    if line:
      return re.sub(r'#.*', '', line).strip()
    else:
      self._reached_eof = True
      return ''

  def _unexpected(self, context='Unexpected line in input'):
    """"Reports that we didn't find the expected context. """
    raise Exception('Line %d: %s' % (self._line_no, context))
