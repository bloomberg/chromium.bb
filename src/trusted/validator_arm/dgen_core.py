#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
The core object model for the Decoder Generator.  The dg_input and dg_output
modules both operate in terms of these classes.
"""

NUM_INST_BITS = 32

def _popcount(int):
    """Returns the number of 1 bits in the input."""
    count = 0
    for bit in range(0, NUM_INST_BITS):
        count = count + ((int >> bit) & 1)
    return count


class BitPattern(object):
    """A pattern for matching strings of bits.  See parse() for syntax."""

    @staticmethod
    def parse(pattern, hi_bit, lo_bit):
        """ Parses a string pattern describing some bits.  The string can
        consist of '1' and '0' to match bits explicitly, 'x' or 'X' to ignore
        bits, '_' as an ignored separator, and an optional leading '~' to
        negate the entire pattern.  Examples:
          10xx0
          1111_xxxx
          ~111x

        The pattern may also optionally be '-', which is equivalent to a
        sequence of 'xxx...xxx' of the requested width.

        Args:
            pattern: a string in the format described above.
            hi_bit: the top of the range matched by this pattern, inclusive.
            lo_bit: the bottom of the range matched by this pattern, inclusive.
        Returns:
            A BitPattern instance that describes the match, and is capable of
            transforming itself to a C expression.
        Raises:
            Exception: the input didn't meet the rules described above.
        """
        num_bits = hi_bit - lo_bit + 1
        # Convert - into a full-width don't-care pattern.
        if pattern == '-':
            return BitPattern.parse('x' * num_bits, hi_bit, lo_bit)

        # Derive the operation type from the presence of a leading tilde.
        if pattern.startswith('~'):
            op = '!='
            pattern = pattern[1:]
        else:
            op = '=='

        # Allow use of underscores anywhere in the pattern, as a separator.
        pattern = pattern.replace('_', '')

        if len(pattern) != num_bits:
            raise Exception('Pattern %s is wrong length for %d:%u'
                % (pattern, hi_bit, lo_bit))

        mask = 0
        value = 0
        for c in pattern:
            if c == '1':
                mask = (mask << 1) | 1
                value = (value << 1) | 1
            elif c == '0':
                mask = (mask << 1) | 1
                value = value << 1
            elif c == 'x' or c == 'X':
                mask = mask << 1
                value = value << 1
            else:
                raise Exception('Invalid characters in pattern %s' % pattern)

        mask = mask << lo_bit
        value = value << lo_bit
        return BitPattern(mask, value, op)

    @staticmethod
    def parse_catch(pattern, hi_bit, lo_bit):
        """"Calls parse with given arguments, and catches exceptions
            raised. Prints raised exceptions and returns None.
        """
        try:
            return BitPattern.parse(pattern, hi_bit, lo_bit);
        except Exception as ex:
            print "Error: %s" % ex
            return None

    def __init__(self, mask, value, op, column=None):
        """Initializes a BitPattern.

        Args:
            mask: an integer with 1s in the bit positions we care about (e.g.
                those that are not X)
            value: an integer that would match our pattern, subject to the mask.
            op: either '==' or '!=', if the pattern is positive or negative,
                respectively.
            column: If specified, the corresponding column information for
                the bit pattern.
        """
        self.mask = mask
        self.value = value
        self.op = op
        self.significant_bits = _popcount(mask)
        self.column = column

    def conflicts(self, other):
        """Returns an integer with a 1 in each bit position that conflicts
        between the two patterns, and 0s elsewhere.  Note that this is only
        useful if the masks and ops match.
        """
        return (self.mask & self.value) ^ (other.mask & other.value)

    def is_complement(self, other):
        """Checks if two patterns are complements of each other.  This means
        they have the same mask and pattern bits, but one is negative.
        """
        return (self.op != other.op
            and self.mask == other.mask
            and self.value == other.value)

    def is_strictly_compatible(self, other):
        """Checks if two patterns are safe to merge using +, but are not ==."""
        if self.is_complement(other):
            return True
        elif self.op == other.op:
            return (self.mask == other.mask
                and _popcount(self.conflicts(other)) == 1)
        return False

    def __add__(self, other):
        """Merges two compatible patterns into a single pattern that matches
        everything either pattern would have matched.
        """
        assert (self == other) or self.is_strictly_compatible(other)

        if self.op == other.op:
            c = self.conflicts(other)
            return BitPattern((self.mask | other.mask) ^ c,
                (self.value | other.value) ^ c, self.op)
        else:
            return BitPattern(0, 0, '==')  # matches anything

    def to_c_expr(self, input):
        """Converts this pattern to a C expression.
        Args:
            input: the name (string) of the C variable to be tested by the
                expression.
        Returns:
            A string containing a C expression.
        """
        # Generate expression corresponding to bit pattern.
        if self.mask == 0:
            value = 'true'
        else:
            value = ('(%s & 0x%08X) %s 0x%08X'
                     % (input, self.mask, self.op, self.value))
        if self.column:
          # Add comment describing test if column is known.
          bits = self._bits_repr()
          bits = bits[self.column[2]:self.column[1]+1]
          bits.reverse()
          return ("%s /* %s(%s:%s) == %s%s */"
                  % (value,
                     self.column[0],
                     self.column[1],
                     self.column[2],
                     '' if self.op == '==' else '~',
                     ''.join(bits)))
        return value

    def __cmp__(self, other):
        """Compares two patterns for sorting purposes.  We sort by
        - # of significant bits, DESCENDING,
        - then mask value, numerically,
        - then value, numerically,
        - and finally op.

        This is also used for equality comparison using ==.
        """
        return (cmp(other.significant_bits, self.significant_bits)
            or cmp(self.mask, other.mask)
            or cmp(self.value, other.value)
            or cmp(self.op, other.op))


    def first_bit(self):
      """Returns the index of the first 0/1 bit in the pattern. or
         None if no significant bits exist for the pattern.
      """
      for i in range(0, NUM_INST_BITS):
        if (self.mask >> i) & 1:
          return i
      return None

    def add_column_info(self, columns):
      """If the bit pattern doesn't have column information, add
         it based on the columns passed in. Otherwise return self.
      """
      if self.column: return self
      for (name, hi_bit, lo_bit) in columns:
        index = self.first_bit()
        if not index: break
        if index >= lo_bit and index <= hi_bit:
          return BitPattern(self.mask, self.value, self.op,
                            (name, hi_bit, lo_bit))
      return self

    def _bits_repr(self):
        """Returns the 0/1/x's of the bit pattern as a list (indexed
           by bit position).
        """
        pat = []
        for i in range(0, NUM_INST_BITS):
            if (self.mask >> i) & 1:
                pat.append(`(self.value >> i) & 1`)
            else:
                pat.append('x')
        return pat

    def __repr__(self):
        """Returns the printable string for the bit pattern."""
        repr = ''.join(reversed(self._bits_repr()))
        if self.op == '!=':
          repr = '~' + repr
        return repr

class Table(object):
    """A table in the instruction set definition.  Each table contains 1+
    columns, and 1+ rows.  Each row contains a bit pattern for each column, plus
    the action to be taken if the row matches."""

    def __init__(self, name, citation):
        """Initializes a new Table.
        Args:
            name: a name for the table, used to reference it from other tables.
            citation: the section in the ISA spec this table was derived from.
        """
        self.name = name
        self.citation = citation
        self.default_row = None
        self._rows = []
        self._columns = []

    def columns(self):
      return self._columns[:]

    def add_column(self, name, hi_bit, lo_bit):
        """Adds a column to the table.

        Because we don't use the column information for very much, we don't give
        it a type -- we store it as a list of tuples.

        Args:
            name: the name of the column (for diagnostic purposes only).
            hi_bit: the leftmost bit included.
            lo_bit: the rightmost bit included.
        """
        self._columns.append( (name, hi_bit, lo_bit) )

    def rows(self, default_also = True):
        """Returns all rows in table (including the default row
           as the last element if requested).
        """
        r = self._rows[:]
        if default_also and self.default_row:
          r.append(self.default_row)
        return r

    def add_default_row(self, action, arch):
        """Adds a default action to use if none of the rows apply.
           Returns True if able to define.
        """
        if self.default_row: return False
        self.default_row = Row([BitPattern(0, 0, "==")], action, arch)
        return True

    def add_row(self, patterns, action, arch):
        """Adds a row to the table.
        Args:
            patterns: a list containing a BitPattern for every column in the
                table.
            action: The action associated  with the row. Must either be
                a DecoderAction or a DecoderMethod.
            arch: a string defining the architectures it applies to. None
                implies all.
        """
        self._rows.append(Row(patterns, action, arch))

    def define_pattern(self, pattern, column):
        """Converts the given input pattern (for the given column) to the
           internal form. Returns None if pattern is bad.
        """
        if column >= len(self._columns): return None
        col = self._columns[column]
        return BitPattern.parse_catch(pattern, col[1], col[2])

    def action_filter(self, names):
        """Returns a table with DecoderActions reduced to the given field names.
           Used to optimize out duplicates, depending on context.
        """
        table = Table(self.name, self.citation)
        table._columns = self._columns
        for r in self._rows:
          table.add_row(r.patterns, r.action.action_filter(names), r.arch)
        if self.default_row:
          table.add_default_row(self.default_row.action.action_filter(names),
                                self.default_row.arch)
        return table

    def add_column_to_rows(self, rows):
      """Add column information to each row, returning a copy of the rows
         with column information added.
      """
      new_rows = []
      for r in rows:
        new_patterns = []
        for p in r.patterns:
          new_patterns.append(p.add_column_info(self._columns))
        new_rows.append(Row(new_patterns, r.action, r.arch))
      return new_rows

    def methods(self):
      """Returns the (sorted) list of methods called by the table."""
      methods = set()
      for r in self.rows(True):
        if r.action.__class__.__name__ == 'DecoderMethod':
          methods.add(r.action)
      return sorted(methods)

class DecoderAction:
  """An action defining a class decoder to apply.

  Corresponds to the parsed decoder action:
      decoder_action ::= id (id (word (id)?)?)?

  Fields are:
      baseline - Name of class decoder used in the baseline.
      actual - Name of the class decoder to use in the validator.
      rule - Name of the rule in ARM manual that defines
             the decoder action.
      pattern - The placement of 0/1's within any instruction
             that is matched by the corresponding rule.
      constraints - Additional constraints that apply to the rule.
  """
  def __init__(self, baseline, actual, rule, pattern, constraints):
    self.baseline = baseline
    self.actual = actual
    self.rule = rule
    self.pattern = pattern
    self.constraints = constraints

  def action_filter(self, names):
    return DecoderAction(
        self.baseline if 'baseline' in names else None,
        self.actual if ('actual' in names or
                        ('actual-not-baseline' in names and
                         self.actual != self.baseline)) else None,
        self.rule if 'rule' in names else None,
        self.pattern if 'pattern' in names else None,
        self.constraints if 'constraints' in names else None)

  def __eq__(self, other):
    return (other.__class__.__name__ == 'DecoderAction'
            and self.baseline == other.baseline
            and self.actual == other.actual
            and self.rule == other.rule
            and self.pattern == other.pattern
            and self.constraints == other.constraints)

  def __cmp__(self, other):
    # Order lexicographically on type/fields.
    value = cmp(other.__class__.__name__, 'DecoderAction')
    if value != 0: return value
    value = cmp(self.baseline, other.baseline)
    if value != 0: return value
    value = cmp(self.actual, other.actual)
    if value != 0: return value
    value = cmp(self.rule, other.rule)
    if value != 0: return value
    value = cmp(self.pattern, other.pattern)
    if value != 0: return value
    return cmp(self.constraints, other.constraints)

  def __hash__(self):
    return (hash('DecoderAction') + hash(self.baseline) +
            hash(self.actual) + hash(self.rule) +
            hash(self.pattern) + hash(self.constraints))

  def __repr__(self):
    if self.actual:
      return '= %s => %s %s %s %s' % (self.baseline, self.actual, self.rule,
                                      self.pattern, self.constraints)
    else:
      return '= %s %s %s %s' % (self.baseline, self.rule,
                               self.pattern, self.constraints)

class DecoderMethod(object):
  """An action defining a parse method to call.

  Corresponds to the parsed decoder method:
      decoder_method ::= '->' id

  Fields are:
      name - The name of the corresponding table (i.e. method) that
          should be used to continue searching for a matching class
          decoder.
  """
  def __init__(self, name):
    self.name = name

  def action_filter(self, unused_names):
    return self

  def __eq__(self, other):
    return (self.__class__.__name__ == 'DecoderMethod'
            and self.name == other.name)

  def __cmp__(self, other):
    # Lexicographically compare types/fields.
    value = cmp(other.__class__.__name__, 'DecoderMethod')
    if value != 0: return value
    return cmp(self.name, other.name)

  def __hash__(self):
    return hash('DecoderMethod') + hash(self.name)

  def __repr__(self):
    return '->%s' % self.name

class Row(object):
    """ A row in a Table."""
    def __init__(self, patterns, action, arch):
        """Initializes a Row.
        Args:
            patterns: a list of BitPatterns that must match for this Row to
                match.
            action: the action to be taken if this Row matches.
            arch: the minimum architecture that this Row can match.
        """
        self.patterns = patterns
        self.action = action
        self.arch = arch

        self.significant_bits = 0
        for p in patterns:
            self.significant_bits += p.significant_bits

    def can_merge(self, other):
        """Determines if we can merge two Rows."""
        if self.action != other.action or self.arch != other.arch:
            return False

        equal_columns = 0
        compat_columns = 0
        for (a, b) in zip(self.patterns, other.patterns):
            if a == b:
                equal_columns += 1
            if a.is_strictly_compatible(b):
                compat_columns += 1

        cols = len(self.patterns)
        return (equal_columns == cols
            or (equal_columns == cols - 1 and compat_columns == 1))

    def __add__(self, other):
        assert self.can_merge(other)  # Caller is expected to check!
        return Row([a + b for (a, b) in zip(self.patterns, other.patterns)],
            self.action, self.arch)

    def __cmp__(self, other):
        """Compares two rows, so we can order pattern matches by specificity.
        """
        return (cmp(self.patterns, other.patterns)
            or cmp(self.action, other.action))

    def __repr__(self):
        return 'Row(%s, %s)' % (repr(self.patterns), repr(self.action))

class Decoder(object):
  """Defines a class decoder which consists of set of tables.

  A decoder has a primary (i.e. start) table to parse intructions (and
  select the proper ClassDecoder), as well as a set of additional
  tables to complete the selection of a class decoder. Instances of
  this class correspond to the internal representation of parsed
  decoder tables recognized by dgen_input.py (see that file for
  details).

  Fields are:
      primary - The entry parse table to find a class decoder.
      tables - The (sorted) set of tables defined by a decoder.

  Note: maintains restriction that tables have unique names.
  """

  def __init__(self):
    self.primary = None
    self._is_sorted = False
    self._tables = []
    self._class_defs = {}

  def add(self, table):
    """Adds the table to the set of tables. Returns true if successful.
    """
    if filter(lambda(t): t.name == table.name, self._tables):
      # Table with name already defined, report error.
      return False
    else:
      if not self._tables:
        self.primary = table
      self._tables.append(table)
      self.is_sorted = False
      return True

  def tables(self):
    """Returns the sorted (by table name) list of tables"""
    if self._is_sorted: return self._tables
    self._tables = sorted(self._tables, key=lambda(tbl): tbl.name)
    self._is_sorted = True
    return self._tables

  def add_class_def(self, cls, supercls):
    """Adds that cls's superclass is supercls. Returns true if able to add.

       Arguments are:
         cls - The class (name) being defined.
         supercls - The class (name) cls is a subclass of.
    """
    if cls in self._class_defs:
      return self._class_defs[cls] == supercls
    else:
      self._class_defs[cls] = supercls
      return True

  def action_filter(self, names):
    """Returns a new set of tables with actions reduced to the set of
    field names.
    """
    decoder = Decoder()
    decoder._tables = sorted([ t.action_filter(names) for t in self._tables ],
                             key=lambda(tbl): tbl.name)
    decoder.primary = filter(
        lambda(t): t.name == self.primary.name, self._tables)[0]
    decoder._class_defs = self._class_defs.copy()
    return decoder

  def base_class(self, cls):
    """Returns the base-most class of cls (or cls if no base class). """
    tried = set()
    while cls in self._class_defs:
      if cls in tried:
        raise Exception('Class %s defined circularly' % cls)
      tried.add(cls)
      cls = self._class_defs[cls]
    return cls

  def decoders(self):
    """Returns the sorted sequence of DecoderAction's defined in the tables."""
    decoders = set()
    for t in self._tables:
        for r in t.rows(True):
            if r.action.__class__.__name__ == 'DecoderAction':
                decoders.add(r.action)
    return sorted(decoders)

  def rules(self):
    """Returns the sorted sequence of DecoderActions that define
       the rule field.
    """
    return sorted(filter(lambda (r): r.rule, self.decoders()))
