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

action         ::= decoder_action | decoder_method | '"'
action_arch    ::= 'arch' ':=' word
action_option  ::= (action_rule |
                    action_pattern |
                    action_safety |
                    action_arch |
                    action_other) ';'
action_options_deprecated ::= (id (word (rule_restrict id?)?)?)?
action_other   ::= word ':=' bit_expr
action_pattern ::= 'pattern' ':=' word rule_restrict?
action_safety  ::= 'safety' ':=' safety_check ('&' safety_check)*
action_rule    ::= 'rule' ':=' id
arch           ::= '(' word+ ')'
bit_check      ::= column '=' bitpattern
bit_expr       ::= bit_expr1 ('if' bit_expr 'else' bit_expr)?  # conditional
bit_expr1      ::= bit_expr2 (('&' bit_expr2)* | ('|' bit_expr2)*)?
bit_expr2      ::= bit_expr3 (('<' | '<=' | '==' | '!=' | '>=' | '>')
                              bit_expr3)?                      # comparisons
bit_expr3      ::= bit_expr4 |                                 # add ops
                   bit_expr3 (('+' bit_expr4) | ('-' bit_expr4))?
bit_expr4      ::= bit_expr5 |                                 # mul ops
                   bit_expr4 (('*' bit_expr5) |
                              ('/' bit_expr5) |
                              ('mod' bit_expr5))?
bit_expr5      ::= bit_expr6 ('=' bitpattern)?                 # bit check
bit_expr6      ::= bit_expr7 (':' bit_expr7)*                  # concat
bit_expr7      ::= bit_expr8 |
                   bit_expr7 ('(' int (':' int)? ')')?         # bit range
bit_expr8      ::= int | id | bit_check | bit_set | '(' bit_expr ')'
bit_set        ::= '{' (bit_expr (',' bit_expr)*)? '}'
bitpattern     ::= word | negated_word
citation       ::= '(' word+ ')'
column         ::= id '(' int (':' int)? ')'
classdef       ::= 'class' word ':' word
decoder        ::= id ('=>' id)?
decoder_action ::= '=" decoder (fields? action_option* |
                                action_options_deprecated arch?)
decoder_method ::= '->' id
default_row    ::= 'else' ':' action
fields         ::= '{' column (',' column)* '}'
footer         ::= '+' '-' '-'
header         ::= "|" column+
int            ::= word     (where word is a sequence of digits)
id             ::= word     (where word is sequence of letters, digits and _)
negated_word   ::= '~' word
parenthesized_exp ::= '(' (word | punctuation)+ ')'
pat_row        ::= pattern+ action
pattern        ::= bitpattern | '-' | '"'
row            ::= '|' (pat_row | default_row)
rule_restrict  ::= ('&' bitpattern)* ('&' 'other' ':' id)?
safety_check   ::= id | bit_expr1 ('=>' id)?     # note: single id only at end.
table          ::= table_desc header row+ footer
table_desc     ::= '+' '-' '-' id citation?

Note that action_options_deprecated is deprecated, and one should generate
a sequence of action_option's. For action_options_depcrected, the interpretation
is as follows:
   id[1] = Arm rule action corresponds to.
   word = Bit pattern of rule.
   rule_restrict = Name defining additional constraints (parse and safety)
          for match.
"""

import re
import dgen_core

# Set the following to True if you want to see each read/pushback of a token.
_TRACE_TOKENS = False

def parse_tables(input):
  """Entry point for the parser.  Input should be a file or file-like."""
  parser = Parser()
  return parser.parse(input)

class Token(object):
  """Holds a (characterized) unit of text for the parser."""

  def __init__(self, kind, value=None):
    self.kind = kind
    self.value = value if value else kind

  def __repr__(self):
    return 'Token(%s, "%s")' % (self.kind, self.value)

# Predefined names corresponding to predefined bit expressions.
_PREDEFINED_CONSTS = {
    # Register Names.
    # TODO(karl): These constants are arm32 specific. We will need to
    # fix this when implementing arm64.
    'NZCV': dgen_core.Literal(16),  # defines conditions registers.
    'None': dgen_core.Literal(32),
    'Pc': dgen_core.Literal(15),
    'Ln': dgen_core.Literal(14),
    'Sp': dgen_core.Literal(13),
    # Boolean values.
    'true': dgen_core.BoolValue(True),
    'false': dgen_core.BoolValue(False)
    }

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
    self._pushed_tokens = []     # Tokens pushed back onto the input stream.
    # Reserved words allowed. Must be ordered such that if p1 != p2 are in
    # the list, and p1.startswith(p2), then p1 must appear before p2.
    self._reserved = ['class', 'else', 'pattern', 'safety', 'rule',
                      'arch', 'other', 'mod', 'if']
    # Punctuation allowed. Must be ordered such that if p1 != p2 are in
    # the list, and p1.startswith(p2), then p1 must appear before p2.
    self._punctuation = ['=>', '->', '-', '+', '(', ')', '==', ':=', '"',
                         '|', '~', '&', '{', '}', ',', ';', '=',':',
                         '>=', '>', '<=', '<', '!=', '*', '/']

  #-------- Recursive descent parse functions corresponding to grammar above.

  def _action(self, last_action):
    """ action ::= decoder_action | decoder_method | '"' """
    if self._next_token().kind == '"':
      self._read_token('"')
      return last_action
    if self._next_token().kind == '=':
      return self._decoder_action()
    elif self._next_token().kind == '->':
      return self._decoder_method()
    else:
      self._unexpected("Row doesn't define an action")

  def _action_arch(self, context):
    """action_arch    ::= 'arch' ':=' arch

       Adds architecture to context."""
    self._read_token('arch')
    self._read_token(':=')
    if not context.define('arch', self._read_token('word').value, False):
      self._unexpected('arch: multiple definitions')

  def _action_option(self, context):
    """action_option  ::= (action_rule | action_pattern |
                           action_safety | action_arch) ';'

       Returns the specified architecture, or None if other option.
       """
    if self._next_token().kind == 'rule':
      self._action_rule(context)
    elif self._next_token().kind == 'pattern':
      self._action_pattern(context)
    elif self._next_token().kind == 'safety':
      self._action_safety(context)
    elif self._next_token().kind == 'arch':
      self._action_arch(context)
    elif self._next_token().kind == 'word':
      self._action_other(context)
    else:
      self._unexpected("Expected action option but not found")
    self._read_token(';')

  def _action_other(self, context):
    """action_other   ::= 'word' ':=' bit_expr

       Recognizes other actions not currently implemented.
       """
    name = self._read_token('word').value
    self._read_token(':=')
    if not context.define(name, self._bit_expr(context), False):
      raise Exception('%s: multiple definitions.' % name)

  def _action_pattern(self, context):
    """action_pattern ::= 'pattern' ':=' bitpattern rule_restrict?

       Adds pattern/parse constraints to the context.
       """
    self._read_token('pattern')
    self._read_token(':=')
    if not context.define('pattern', self._bitpattern32(), False):
      raise Exception('pattern: multiple definitions.')
    if self._next_token().kind == '&':
      self._rule_restrict(context)

  def _action_safety(self, context):
    """action_safety  ::= 'safety' ':=' safety_check ('&' safety_check)*

       Adds safety constraints to the context.
       """
    self._read_token('safety')
    self._read_token(':=')
    checks = [ self._safety_check(context) ]
    while self._next_token().kind == '&':
      self._read_token('&')
      checks.append(self._safety_check(context))
    if not context.define('safety', checks, False):
      raise Exception('safety: multiple definitions')

  def _action_rule(self, context):
    """action_rule    ::= 'rule' ':=' id

       Adds rule name to the context.
       """
    self._read_token('rule')
    self._read_token(':=')
    if not context.define('rule', self._id(), False):
      raise Exception('rule: multiple definitions')

  def _arch(self):
    """ arch ::= '(' word+ ')' """
    return ' '.join(self._parenthesized_exp())

  def _bit_check(self, context):
    """ bit_check      ::= column '=' bitpattern """
    column = None
    if self._is_column():
      column = self._column()
      self._read_token('=')
    elif self._is_name_equals():
      name = self._id()
      column = context.find(name)
      if not column:
        self._unexpected("Can't find column definition for %s" % name)
      self._read_token('=')
    else:
      self._unexpected("didn't find bit pattern check")
    pattern = dgen_core.BitPattern.parse(self._bitpattern(), column)
    if not pattern:
      self._unexpected("bit pattern check malformed")
    return pattern

  def _bit_expr(self, context):
    """bit_expr ::= bit_expr1 ('if' bit_expr 'else' bit_expr)?"""
    test = self._bit_expr1(context)
    if not self._next_token().kind == 'if': return test
    self._read_token('if')
    then_value = self._bit_expr(context)
    self._read_token('else')
    else_value = self._bit_expr(context)
    return dgen_core.IfThenElse(test, then_value, else_value)

  def _bit_expr1(self, context):
    """bit_expr1 ::= bit_expr2 (('&' bit_expr2)* | ('|' bit_expr2)*)"""
    value = self._bit_expr2(context)
    if self._next_token().kind == '&':
      args = [value]
      while self._next_token().kind == '&':
        self._read_token('&')
        args.append(self._bit_expr2(context))
      value = dgen_core.AndExp(args)
    elif self._next_token().kind == '|':
      args = [value]
      while self._next_token().kind == '|':
        self._read_token('|')
        args.append(self._bit_expr2(context))
      value = dgen_core.OrExp(args)
    return value

  def _bit_expr2(self, context):
    """bit_expr2 ::= bit_expr3 (('<' | '<=' | '==' | '!=' | '>=' | '>')
                                bit_expr3)? """
    value = self._bit_expr3(context)
    for op in ['<', '<=', '==', '!=', '>=', '>']:
      if self._next_token().kind == op:
        self._read_token(op)
        return dgen_core.CompareExp(op, value, self._bit_expr3(context))
    return value

  def _bit_expr3(self, context):
    """bit_expr3 ::= bit_expr4 |
                     bit_expr3 (('+' bit_expr4) | ('-' bit_expr4))?"""
    value = self._bit_expr4(context)
    while self._next_token().kind in ['+', '-']:
      op = self._read_token().value
      value = dgen_core.AddOp(op, value, self._bit_expr4(context))
    return value

  def _bit_expr4(self, context):
    """bit_expr4 ::= bit_expr5 |
                     bit_expr4 (('*' bit_expr5) |
                                ('/' bit_expr5) |
                                ('mod' bit_expr5))?"""
    value = self._bit_expr5(context)
    while self._next_token().kind in ['*', '/', 'mod']:
      op = self._read_token().value
      value = dgen_core.MulOp(op, value, self._bit_expr5(context))
    return value

  def _bit_expr5(self, context):
    """bit_expr5 ::= bit_expr6 ('=' bitpattern)"""
    bits = self._bit_expr6(context)
    if self._next_token().kind != '=': return bits
    self._read_token('=')
    bitpat = self._bitpattern()
    pattern = dgen_core.BitPattern.parse_catch(bitpat, bits)
    if not pattern:
      self._unexpected('Pattern mismatch in %s = %s' % (bits, bitpat))
    else:
      return pattern

  def _bit_expr6(self, context):
    """bit_expr6 ::= bit_expr7 (':' bit_expr7)*"""
    value = self._bit_expr7(context)
    if self._next_token().kind != ':': return value
    values = [ value ]
    while self._next_token().kind == ':':
      self._read_token(':')
      values.append(self._bit_expr7(context))
    return dgen_core.Concat(values)

  def _bit_expr7(self, context):
    """bit_expr7 ::= bit_expr8 |
                     bit_expr7 ('(' int (':' int)? ')')?"""
    value = self._bit_expr8(context)
    while self._next_token().kind == '(':
      self._read_token('(')
      hi_bit = self._int()
      lo_bit = hi_bit
      if self._next_token().kind == ':':
        self._read_token(':')
        lo_bit = self._int()
      self._read_token(')')
      value = dgen_core.BitField(value, hi_bit, lo_bit)
    return value

  def _bit_expr8(self, context):
    """bit_expr8 ::= int | id | bit_check | bit_set | '(' bit_expr ')'"""
    if self._is_int():
      return dgen_core.Literal(self._int())
    elif self._is_bit_check():
      return self._bit_check(context)
    elif self._next_token().kind == '{':
      return self._bit_set(context)
    elif self._next_token().kind == '(':
      self._read_token('(')
      value = self._bit_expr(context)
      self._read_token(')')
      return dgen_core.ParenthesizedExp(value)
    else:
      name = self._id()
      value = context.find(name)
      if not value:
        value = _PREDEFINED_CONSTS.get(name)
        if not value:
          self._unexpected("Can't find definition for %s" % name)
      return dgen_core.IdRef(name, value)

  def _bit_set(self, context):
    """bit_set        ::= '{' (bit_expr (',' bit_expr)*)? '}'"""
    values = []
    self._read_token('{')
    if not self._next_token().kind == '}':
      values.append(self._bit_expr(context))
      while self._next_token().kind == ',':
        self._read_token(',')
        values.append(self._bit_expr(context))
    self._read_token('}')
    return dgen_core.BitSet(values)

  def _bitpattern(self):
    """ bitpattern     ::= 'word' | negated_word """
    return (self._negated_word() if self._next_token().kind == '~'
            else self._read_token('word').value)

  def _bitpattern32(self):
    """Returns a bit pattern with 32 bits."""
    pattern = self._bitpattern()
    if pattern[0] == '~':
      if len(pattern) != 33:
        self._unexpected("Bit pattern  %s length != 32" % pattern)
    elif len(pattern) != 32:
      self._unexpected("Bit pattern  %s length != 32" % pattern)
    return pattern

  def _citation(self):
    """ citation ::= '(' word+ ')' """
    return ' '.join(self._parenthesized_exp())

  def _column(self):
    """column         ::= id '(' int (':' int)? ')'

       Reads a column and returns the correspond representation.
    """
    name = self._read_token('word').value
    self._read_token('(')
    hi_bit = self._int()
    lo_bit = hi_bit
    if self._next_token().kind == ':':
      self._read_token(':')
      lo_bit = self._int()
    self._read_token(')')
    return dgen_core.BitField(name, hi_bit, lo_bit)

  def _classdef(self, decoder):
    """ classdef       ::= 'class' word ':' word """
    self._read_token('class')
    class_name = self._read_token('word').value
    self._read_token(':');
    class_superclass = self._read_token('word').value
    if not decoder.add_class_def(class_name, class_superclass):
      self._unexpected('Inconsistent definitions for class %s' % class_name)

  def _decoder(self):
    """ decoder        ::= id ('=>' id)? """
    baseline = self._id()
    actual = baseline
    if self._next_token().kind == '=>':
      self._read_token('=>')
      actual = self._id()
    return (baseline, actual)

  def _decoder_action_options(self, context):
    """Parses 'action_options*'."""
    while True:
      if self._is_action_option():
        self._action_option(context)
      else:
        return

  def _decoder_action(self):
    """decoder_action ::= '=" decoder (fields? action_option* |
                                       action_options_deprecated arch?)
    """
    self._read_token('=')
    (baseline, actual) = self._decoder()
    action = dgen_core.DecoderAction(baseline, actual)
    context = action.st
    if self._next_token().kind == '{':
      fields = self._fields(context)
      if not context.define('fields', fields, False):
        raise Exception('multiple field definitions.')
      self._decoder_action_options(context)
    elif self._is_action_option():
      self._decoder_action_options(context)
    else:
      context.define('rule', self._read_id_or_none(True))
      context.define('pattern', self._read_id_or_none(False))
      self._rule_restrict(context)
      other_restrictions = self._read_id_or_none(True)
      if other_restrictions:
        context.define('safety', [other_restrictions])
      if self._next_token().kind == '(':
        context.define('arch', self._arch())
    return action

  def _decoder_method(self):
    """ decoder_method ::= '->' id """
    self._read_token('->')
    name = self._id()
    return dgen_core.DecoderMethod(name)

  def _default_row(self, table, last_action):
    """ default_row ::= 'else' ':' action """
    self._read_token('else')
    self._read_token(':')
    action = self._action(last_action)
    if not table.add_default_row(action):
      self._unexpected('Unable to install row default')
    return (None, action)

  def _fields(self, context):
    """fields         ::= '{' column (',' column)* '}'"""
    fields = []
    self._read_token('{')
    field = self._column()
    fields.append(field)
    self._field_define(field, field.name().name(), context)
    while self._next_token().kind == ',':
      self._read_token(',')
      field = self._column()
      self._field_define(field, field.name().name(), context)
      fields.append(field)
    self._read_token('}')
    return fields

  def _footer(self):
    """ footer ::= '+' '-' '-' """
    self._read_token('+')
    self._read_token('-')
    self._read_token('-')

  def _header(self, table):
    """ header ::= "|" column+ """
    self._read_token('|')
    self._add_column(table)
    while self._is_column():
      self._add_column(table)

  def _int(self):
    """ int ::= word

    Int is a sequence of digits. Returns the corresponding integer.
    """
    if self._is_int():
      return int(self._read_token('word').value)
    else:
      self._unexpected(
          'integer expected but found "%s"' % self._next_token())

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

  def _named_value(self, context):
    """named_value    ::= id ':=' bit_expr."""
    name = self._id()
    self._read_token(':=')
    value = self._bit_expr(context)
    self._field_define(value, name, context)
    return value

  def _negated_word(self):
    """ negated_word ::= '~' 'word' """
    self._read_token('~')
    return '~' + self._read_token('word').value

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

  def _pat_row(self, table, last_patterns, last_action):
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

    action = self._action(last_action)
    table.add_row(expanded_patterns, action)
    return (patterns, action)

  def _pattern(self, last_pattern):
    """ pattern        ::= bitpattern | '-' | '"'

    Arguments are:
      last_pattern: The pattern for this column from the previous line.
    """
    if self._next_token().kind == '"':
      self._read_token('"')
      return last_pattern
    if self._next_token().kind in ['-', 'word']:
      return self._read_token().value
    return self._bitpattern()

  def _row(self, table, last_patterns=None, last_action=None):
    """ row ::= '|' (pat_row | default_row)

    Passed in sequence of patterns and action from last row,
    and returns list of patterns and action from this row.
    """
    self._read_token('|')
    if self._next_token().kind == 'else':
      return self._default_row(table, last_action)
    else:
      return self._pat_row(table, last_patterns, last_action)

  def _rule_restrict(self, context):
    """ rule_restrict  ::= ('&' bitpattern)* ('&' 'other' ':' id)? """

    restrictions = context.find('constraints')
    if not restrictions:
      context.define('constraints', dgen_core.RuleRestrictions())
    while self._next_token().kind == '&':
      self._read_token('&')
      if self._next_token().kind == 'other':
        self._read_token('other')
        self._read_token(':')
        restrictions.safety = self._id()
        return
      else:
        restrictions.add(self._bitpattern32())

  def _safety_check(self, context):
    """safety_check   ::= id | bit_expr ('=>' id)?

       Parses safety check and returns it.
       """
    if self._is_name_semicolon():
      # This is a special case where we are picking up a instruction
      # tester suffix.
      check = self._id()
    else:
      check = self._bit_expr1(context)
      if self._next_token().kind == '=>':
        self._read_token('=>')
        name = self._id()
        check = dgen_core.SafetyAction(check, name)
    return check

  def _table(self, decoder):
    """ table ::= table_desc header row+ footer """
    table = self._table_desc()
    self._header(table)
    (pattern, action) = self._row(table)
    while not self._next_token().kind == '+':
      (pattern, action) = self._row(table, pattern, action)
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

  #------ Syntax checks ------

  def _at_eof(self):
    """Returns true if next token is the eof token."""
    return self._next_token().kind == 'eof'

  def _is_action_option(self):
    """Returns true if the input matches an action_option.

       Note: We assume that checking for a word, followed by an assignment
       is sufficient.
       """
    matches = False
    if self._next_token().kind in ['pattern', 'rule', 'safety', 'arch', 'word']:
      token = self._read_token()
      if self._next_token().kind == ':=':
        matches = True
      self._pushback_token(token)
    return matches

  def _is_bit_check(self):
    """Returns true if a bit check appears next on the input stream.

       Assumes that if a column if found, it must be a bit check.
       """
    return self._is_column_equals() or self._is_name_equals()

  def _is_column(self):
    """Returns true if input defines a column (pattern name).

       column         ::= id '(' int (':' int)? ')'
       """
    (tokens, matches) = self._is_column_tokens()
    self._pushback_tokens(tokens)
    return matches

  def _is_column_equals(self):
    """ """
    (tokens, matches) = self._is_column_tokens()
    if self._next_token().kind != '=':
      matches = False
    self._pushback_tokens(tokens)
    return matches

  def _is_column_tokens(self):
    """Collects the sequence of tokens defining:
       column         ::= id '(' int (':' int)? ')'
       """
    # Try to match sequence of tokens, saving tokens as processed
    tokens = []
    matches = False
    if self._next_token().kind == 'word':
      tokens.append(self._read_token('word'))
      if self._next_token().kind == '(':
        tokens.append(self._read_token('('))
        if self._next_token().kind == 'word':
          tokens.append(self._read_token('word'))
          if self._next_token().kind == ':':
            tokens.append(self._read_token(':'))
            if self._next_token().kind == 'word':
              tokens.append(self._read_token('word'))
          if self._next_token().kind == ')':
            matches = True
    return (tokens, matches)

  def _is_int(self):
    """Tests if an integer occurs next."""
    if self._next_token().kind != 'word': return False
    word = self._next_token().value
    return re.match(r'^([0-9]+)$', word)

  def _is_name_equals(self):
    matches = False
    if self._next_token().kind == 'word':
      token = self._read_token('word')
      if self._next_token().kind == '=':
        matches = True
      self._pushback_token(token)
    return matches

  def _is_name_semicolon(self):
    matches = False
    if self._next_token().kind == 'word':
      token = self._read_token('word')
      if self._next_token().kind == ';':
        matches = True
      self._pushback_token(token)
    return matches

  #------ Helper functions.

  def _add_column(self, table):
    """Adds a column to a table, and verifies that it isn't repeated."""
    column = self._column()
    if not table.add_column(column):
      self._unexpected('In table %s, column %s is repeated' %
                       (table.name, column.name()))

  def _field_define(self, column, col_name, context):
    """Install column into the context."""
    if not context.define(col_name,  column):
      raise Exception('column %s: multiple definitions' % col_name)

  def _read_id_or_none(self, read_id):
    if self._next_token().kind in ['|', '+', '(']:
      return None
    name = self._id() if read_id else self._read_token('word').value
    return None if name and name == 'None' else name

  #------ Tokenizing the input stream ------

  def _read_token(self, kind=None):
    """Reads and returns the next token from input."""
    token = self._next_token()
    self._token = None
    if kind and kind != token.kind:
      self._unexpected('Expected "%s" but found "%s"'
                       % (kind, token.kind))
    if _TRACE_TOKENS:
      print "Read %s" % token
    return token

  def _next_token(self):
    """Returns the next token from the input."""
    # First seee if cached.
    if self._token: return self._token

    if self._pushed_tokens:
      self._token = self._pushed_tokens.pop()
      return self._token

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

      # See if reserved keyword.
      for key in self._reserved:
        index = word.find(key)
        if index == 0:
          # Found reserved keyword. Verify at end, or followed
          # by punctuation.
          rest = word[len(key):]
          if not rest or 0 in [rest.find(p) for p in self._punctuation]:
            self._token = Token(key)
            return self._token

      # if reached, word doesn't contain any reserved words
      # punctuation, so return it.
      self._token = Token('word', word)
    else:
      # No more tokens found, assume eof.
      self._token = Token('eof')
    return self._token

  def _pushback(self, word):
    """Puts word back onto the list of words."""
    if word:
      self._words.insert(0, word)

  def _pushback_token(self, token):
    """Puts token back on to the input stream."""
    if _TRACE_TOKENS:
      print "pushback %s" % token
    if self._token:
      self._pushed_tokens.append(self._token)
      self._token = token
    else:
      self.token = token

  def _pushback_tokens(self, tokens):
    """Puts back the reversed list of tokens on to the input stream."""
    while tokens:
      self._pushback_token(tokens.pop())

  def _read_line(self):
    """Reads the next line of input, and returns it. Otherwise None."""
    self._line_no += 1
    line = self.input.readline()
    if line:
      return re.sub(r'#.*', '', line).strip()
    else:
      self._reached_eof = True
      return ''

  #-------- Error reporting ------

  def _unexpected(self, context='Unexpected line in input'):
    """"Reports that we didn't find the expected context. """
    raise Exception('Line %d: %s' % (self._line_no, context))
