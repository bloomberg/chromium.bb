#!/usr/bin/python
#
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

"""
The core object model for the Decoder Generator.  The dg_input and
dg_output modules both operate in terms of these classes.
"""

NUM_INST_BITS = 32

NEWLINE_STR="""
"""

def _popcount(int):
    """Returns the number of 1 bits in the input."""
    count = 0
    for bit in range(0, NUM_INST_BITS):
        count = count + ((int >> bit) & 1)
    return count

def neutral_repr(value):
  """Returns a neutral representation for the value.

     Used to remove identifier references from values, so that we can
     merge rows/actions of the table.
     """
  if (isinstance(value, BitExpr) or isinstance(value, SymbolTable) or
      isinstance(value, Row) or isinstance(value, DecoderAction)):
    return value.neutral_repr()
  elif isinstance(value, list):
    return [ neutral_repr(v) for v in value ]
  else:
    return repr(value)

class BitExpr(object):
  """Define a bit expression."""

  def negate(self):
    """Returns the negation of self."""
    return NegatedTest(self)

  def to_bitfield(self, options={}):
    """Returns the bit field (i.e. sequence of bits) described by the
       BitExpr. Returns an instance of BitField.
       """
    # Default implementation is to convert it by adding unsigned
    # int mask.
    return BitField(self, NUM_INST_BITS - 1, 0)

  def to_bool(self, options={}):
    """Returns a string describing this as a C++ boolean
       expression."""
    return "(%s != 0)" % self.to_uint32(options)

  def to_commented_bool(self, options={}):
    """Returns a string describing this as a C++ boolean expression,
       with a comment describing the corresponding BitExpr it came
       from."""
    return '%s /* %s */' % (self.to_bool(options), repr(self))

  def to_register(self, options={}):
    """Returns a string describing this as a C++ Register."""
    return 'Register(%s)' % self.to_uint32(options)

  def to_commented_register(self, options={}):
    """Returns a string describing this as a C++ Register expression,
       with a comment describing the corresponding BitExpr it came
       from."""
    return '%s /* %s */' % (self.to_register(options), repr(self))

  def to_register_list(self, options={}):
    """Returns a string describing this as a C++ RegisterList
       expression."""
    raise Exception("to_register_list not defined for %s." % type(self))

  def to_commmented_register_list(self, options={}):
    """Returns a string describing this as a C++ RegisterList
       Expression, with a comment describing the corresponding BitExpr
       it came from."""
    return '%s /* %s */' % (self.to_register_list(options), repr(self))

  def to_uint32(self, options={}):
    """Returns a string describing this as a C++ uint32_t
    expression."""
    raise Exception("to_uint32 not defined for %s." % type(self))

  def to_commented_uint32(self, options={}):
    """Returns a string describing this as a C++ uint32_t expression,
       with a comment describing the corresponding BitExpr it came
       from."""
    return '%s /* %s */' % (self.to_uint32(options), repr(self))

  def neutral_repr(self):
    """Like repr(self) except identifier references are replaced with
      their definition.

       Used to define a form for comparison/hashing that is neutral to
       the naming conventions used in the expression.
       """
    raise Exception("neutral_repr not defined for %s" % type(self))

  def __hash__(self):
    return hash(neutral_repr(self))

  def __cmp__(self, other):
    return cmp(neutral_repr(self), neutral_repr(other))

class IdRef(BitExpr):
  """References an (already defined) name, and the value associated
     with the name.
     """

  def __init__(self, name, value=None):
    self._name = name
    self._value = value

  def name(self):
    return self._name

  def value(self):
    return self._value

  def to_bitfield(self, options={}):
    return self._value.to_bitfield(options)

  def to_bool(self, options={}):
    self._value.to_bool(options)

  def to_register(self, options={}):
    self._value.to_register(options)

  def to_register_list(self, options={}):
    self._value.to_register_list(options)

  def to_uint32(self, options={}):
    return self._value.to_uint32(options)

  def __repr__(self):
    return '%s' % self._name

  def neutral_repr(self):
    return self._value.neutral_repr()

class AndExp(BitExpr):
  """Models an anded expression."""

  def __init__(self, args):
    if not isinstance(args, list) or len(args) < 2:
      raise Exception(
          "AndExp(%s) expects at least two elements" % args)
    self._args = args

  def args(self):
    return self._args[:]

  def to_bool(self, options={}):
    value = '(%s)' % self._args[0].to_bool(options)
    for arg in self._args[1:]:
      value = '%s && (%s)' % (value, arg.to_bool(options))
    return value

  def to_uint32(self, options={}):
    value = self.args[0].to_uint32(options)
    for arg in self._args[1:]:
      value = '%s & %s' % (value, arg.to_uint32(options))
    return '(%s)' % value

  def to_register(self, options={}):
    raise Exception("to_register not defined for %s" % self)

  def to_register_list(self, options={}):
    raise Exception("to_register_list not defined for %s" % self)

  def __repr__(self):
    return ' && '.join([repr(a) for a in self._args])

  def neutral_repr(self):
    return ' && '.join([neutral_repr(a) for a in self._args])

class OrExp(BitExpr):
  """Models an or-ed expression."""

  def __init__(self, args):
    if not isinstance(args, list) or len(args) < 2:
      raise Exception(
          "OrExp(%s) expects at least two elements" % args)
    self._args = args

  def args(self):
    return self._args[:]

  def to_bool(self, options={}):
    value = '(%s)' % self._args[0].to_bool(options)
    for arg in self._args[1:]:
      value = '%s || (%s)' % (value, arg.to_bool(options))
    return value

  def to_register(self, options={}):
    raise Exception("to_register not defined for %s" % self)

  def to_register_list(self, options={}):
    raise Exception("to_register_list not defined for %s" % self)

  def to_uint32(self, options={}):
    value = self.args[0].to_uint32(options)
    for arg in self._args[1:]:
      value = '%s | %s' % (value, arg.to_uint32(options))
    return '(%s)' % value

  def __repr__(self):
    return ' || '.join([repr(a) for a in self._args])

  def neutral_repr(self):
    return ' || '.join([neutral_repr(a) for a in self._args])

# Defines the negated comparison operator.
_NEGATED_COMPARE_OP = {
    '<': '>=',
    '<=': '>',
    '==': '!=',
    '!=': '==',
    '>=': '<',
    '>': '<=',
}

class CompareExp(BitExpr):
  """Models the comparison of two values."""

  def __init__(self, op, arg1, arg2):
    if not _NEGATED_COMPARE_OP.get(op):
      raise Exception("Unknown compare operator: %s" % op)
    self._op = op
    self._args = [arg1, arg2]

  def op(self):
    return self._op

  def args(self):
    return self._args[:]

  def negate(self):
    return CompareExp(_NEGATED_COMPARE_OP[self.op],
                      self._args[0], self._args[1])

  def to_bool(self, options={}):
    return '((%s) %s (%s))' % (self._args[0].to_uint32(options),
                               self._op,
                               self._args[1].to_uint32(options))

  def to_register(self, options={}):
    raise Exception("to_register not defined for %s" % self)

  def to_register_list(self, options={}):
    raise Exception("to_register_list not defined for %s" % self)

  def to_uint32(self, options={}):
    raise Exception("to_uint32 not defined for %s" % self)

  def __repr__(self):
    return '%s %s %s' % (repr(self._args[0]),
                         self._op,
                         repr(self._args[1]))

  def neutral_repr(self):
    # Note: We canconicalize the tests to improve chances that we
    # merge more expressions.
    arg1 = neutral_repr(self._args[0])
    arg2 = neutral_repr(self._args[1])
    if self._op in ['==', '!=']:
      # Order arguments based on comparison value.
      cmp_value = cmp(arg1, arg2)
      if cmp_value < 0:
        return '%s %s %s' % (arg1, self._op, arg2)
      elif cmp_value > 0:
        return '%s %s %s' % (arg2, self._op, arg1)
      else:
        # comparison against self can be simplified.
        return BoolValue(self._op == '==').neutral_repr()
    elif self._op in ['>', '>=']:
      return '%s %s %s' % (arg2, _NEGATED_COMPARE_OP.get(self._op), arg1)
    else:
      # Assume in canonical order.
      return '%s %s %s' % (arg1, self._op, arg2)

class AddOp(BitExpr):
  """Models an additive operator."""

  def __init__(self, op, arg1, arg2):
    if op not in ['+', '-']:
      raise Exception("Not add op: %s" % op)
    self._op = op
    self._args = [arg1, arg2]

  def args(self):
    return self._args[:]

  def to_register_list(self, options={}):
    rl = self._args[0].to_register_list(options)
    if self._op == '+':
      return '%s.Add(%s)' % (rl, self._args[1].to_register(options))
    elif self._op == '-':
      return '%s.Remove(%s)' % (rl, self._args[1].to_register(options))
    else:
      raise Exception("Bad op %s" % self._op)

  def to_uint32(self, options={}):
    return '%s %s %s' % (self._args[0].to_uint32(options),
                         self._op,
                         self._args[1].to_uint32(options))

  def __repr__(self):
    return '%s %s %s' % (repr(self._args[0]),
                         self._op,
                         repr(self._args[1]))
  def neutral_expr(self):
    return '%s %s %s' % (neutral_repr(self._args[0]),
                         self._op,
                         neutral_repr(self._args[1]))

# Defines the c++ operator for the given mulop.
_CPP_MULOP = {
    '*': '*',
    '/': '/',
    'mod': '%%',
}

class MulOp(BitExpr):
  """Models an additive operator."""

  def __init__(self, op, arg1, arg2):
    if not _CPP_MULOP.get(op):
      raise Exception("Not mul op: %s" % op)
    self._op = op
    self._args = [arg1, arg2]

  def args(self):
    return self._args[:]

  def to_uint32(self, options={}):
    return '%s %s %s' % (self._args[0].to_uint32(options),
                         _CPP_MULOP[self._op],
                         self._args[1].to_uint32(options))

  def __repr__(self):
    return '%s %s %s' % (repr(self._args[0]),
                         self._op,
                         repr(self._args[1]))
  def neutral_expr(self):
    return '%s %s %s' % (neutral_repr(self._args[0]),
                         self._op,
                         neutral_repr(self._args[1]))

class Concat(BitExpr):
  """Models a value generated by concatentation bitfields."""

  def __init__(self, args):
    if not isinstance(args, list) or len(args) < 2:
      raise Exception(
          "Concat(%s) expects at least two arguments" % args)
    self._args = args

  def args(self):
    return self._args[:]

  def to_uint32(self, options={}):
    value = self._args[0].to_uint32()
    for arg in self._args[1:]:
      bitfield = arg.to_bitfield(options)
      value = ("(((%s) << %s) | %s" %
               (value, bitfield.num_bits(), bitfield.to_uint32()))
    return value

  def __repr__(self):
    return ':'.join([repr(a) for a in self._args])

  def neutral_repr(self):
    return ':'.join([a.neutral_repr() for a in self._args])

class BitSet(BitExpr):
  """Models a set of expressions."""

  def __init__(self, values):
    self._values = values

  def args(self):
    return self._values[:]

  def to_register_list(self, options={}):
    code = 'RegisterList()'
    for value in self._values:
      code = '%s.Add(%s)' % (code, value.to_register(options))
    return code

  def __repr__(self):
    return '{%s}' % ','.join([repr(v) for v in self._values])

  def neutral_repr(self):
    return '{%s}' % ','.join([neutral_repr(v) for v in self._values])

class FunctionCall(BitExpr):
  """Abstract class defining an (external) function call."""

  def __init__(self, name, args):
    self._name = name
    self._args = args

  def name(self):
    return self._name

  def args(self):
    return self._args[:]

  def to_bitfield(self, options={}):
    raise Exception('to_bitfield not defined for %s' % self)

  def to_bool(self, options={}):
    return self._to_call(options)

  def to_register(self, options={}):
    return self._to_call(options)

  def to_register_list(self, options={}):
    return self._to_call(options)

  def to_uint32(self, options={}):
    return self._to_call(options)

  def __repr__(self):
    return "%s(%s)" % (self._name,
                       ', '.join([repr(a) for a in self._args]))

  def neutral_repr(self):
    return "%s(%s)" % (self._name,
                       ', '.join([neutral_repr(a) for a in self._args]))

  def _to_call(self, options={}):
    """Generates a call to the external function."""
    return '%s(%s)' % (self._name,
                       ', '.join([a.to_uint32(options) for a in self._args]))

class InSet(BitExpr):
  """Abstract class defining set containment."""

  def __init__(self, value, bitset):
    self._value = value
    self._bitset = bitset

  def value(self):
    """Returns the value to test membership on."""
    return self._value

  def bitset(self):
    """Returns the set of values to test membership on."""
    return self._bitset

  def to_bitfield(self, options={}):
    raise Exception("to_bitfield not defined for %s" % self)

  def to_bool(self, options={}):
    return self._simplify().to_bool(options)

  def to_register(self, options={}):
    raise Exception("to_register not defined for %s" % self)

  def to_register_list(self, options={}):
    raise Exception("to_register_list not defined for %s" % self)

  def to_uint32(self, options={}):
    return self._simplify().to_uint32(options)

  def neutral_repr(self):
    return self._simplify().neutral_repr()

  def _simplify(self):
    """Returns the simplified or expression that implements the
       membership tests."""
    args = self._bitset.args()
    if not args: return BoolValue(False)
    if len(args) == 1: return self._simplify_test(args[0])
    return OrExp([self._simplify_test(a) for a in args])

  def _simplify_test(self, arg):
    """Returns how to test if the value matches arg."""
    raise Exception("InSet._simplify_test not defined for type %s" % type(self))

class InUintSet(InSet):
  """Models testing a value in a set of integers."""

  def __init__(self, value, bitset):
    Inset.__init__(self, value, bitset)

  def _simplify_test(self, arg):
    return CompareExpr("==", self._value, arg)

  def repr(self):
    return "%s in %s" % (self._value, self._bitset)

class InBitSet(InSet):
  """Models testing a value in a set of bit patterns"""

  def __init__(self, value, bitset):
    InSet.__init__(self, value, bitset)
    # Before returning, be sure the value/bitset entries correctly
    # correspond, by forcing construction of the simplified expression.
    self.neutral_repr()

  def _simplify_test(self, arg):
    return BitPattern.parse(repr(arg), self._value)

  def repr(self):
    return "%s in bitset %s" % (self._value, self._bitset)

class IfThenElse(BitExpr):
  """Models a conditional expression."""

  def __init__(self, test, then_value, else_value):
    self._test = test
    self._then_value = then_value
    self._else_value = else_value

  def test(self):
    return self._test

  def then_value(self):
    return self._then_value

  def else_value(self):
    return self._else_value

  def negate(self):
    return IfThenElse(self._test, self._else_value, self._then_value)

  def to_bool(self, options={}):
    return "(%s ? %s : %s)" % (self._test.to_bool(options),
                               self._then_value.to_bool(options),
                               self._else_value.to_bool(options))

  def to_uint32(self, options={}):
    return "(%s ? %s : %s)" % (self._test.to_bool(options),
                               self._then_value.to_uint32(options),
                               self._else_value.to_uint32(options))

  def __repr__(self):
    return '%s?%s:%s' % (repr(self._test),
                         repr(self._then_value),
                         repr(self._else_value))

  def neutral_repr(self):
    return '%s?%s:%s' % (neutral_repr(self._test),
                         neutral_repr(self._then_value),
                         neutral_repr(self._else_value))

class ParenthesizedExp(BitExpr):
  """Models a parenthesized expression."""

  def __init__(self, exp):
    self._exp = exp

  def exp(self):
    return self._exp

  def negate(self):
    value = self._exp.negate()
    return ParenthesizedExp(value)

  def to_bitfield(self, options={}):
    return self._exp.to_bitfield(options)

  def to_bool(self, options={}):
    return '(%s)' % self._exp.to_bool(options)

  def to_register(self, options={}):
    return '(%s)' % self._exp.to_register(options)

  def to_register_list(self, options={}):
    return '(%s)' % self._exp.to_register_list(options)

  def to_uint32(self, options={}):
    return '(%s)' % self._exp.to_uint32(options)

  def __repr__(self):
    return '(%s)' % repr(self._exp)

  def neutral_repr(self):
    return '(%s)' % neutral_repr(self._exp)

class Literal(BitExpr):
  """Models a literal unsigned integer."""

  def __init__(self, value):
    if not isinstance(value, int):
      raise Exception("Can't create literal from %s" % value)
    self._value = value

  def value(self):
    return self._value

  def to_uint32(self, options={}):
    return repr(self._value)

  def __repr__(self):
    return repr(self._value)

  def neutral_repr(self):
    return neutral_repr(self._value)

class BoolValue(BitExpr):
  """Models true and false."""

  def __init__(self, value):
    if not isinstance(value, bool):
      raise Exception("Can't create boolean value from %s" % value)
    self._value = value

  def value(self):
    return self._value

  def to_bool(self, options={}):
    return 'true' if self._value else 'false'

  def to_uint32(self, options={}):
    return 1 if self._value else 0

  def __repr__(self):
    return self.to_bool()

  def neutral_repr(self):
    return self.to_bool()

class NegatedTest(BitExpr):
  """Models a negated (test) value."""

  def __init__(self, test):
    self._test = test

  def negate(self):
    return self._test

  def to_bitfield(self, options={}):
    raise Exception("to_bitfield not defined for %s" % self)

  def to_bool(self, options={}):
    return "!(%s)" % self._test.to_bool(options)

  def to_register(self, options={}):
    raise Exception('to_register not defined for %s' % self)

  def to_uint32(self, options={}):
    return "((uint32_t) %s)" % self.to_bool(options)

  def __repr__(self):
    return 'not %s' % self._test

  def neutral_repr(self):
    return 'not %s' % self._test.neutral_repr()

class BitField(BitExpr):
  """Defines a bitfield within an instruction."""

  def __init__(self, name, hi, lo, options={}):
    if not isinstance(name, BitExpr):
      # Unify non-bit expression bitfield to corresponding bitfield
      # version, so that there is a uniform representation.
      name = name if isinstance(name, str) else repr(name)
      name = IdRef(name, Instruction())
    self._name = name
    self._hi = hi
    self._lo = lo
    max_bits = self._max_bits(options)
    if not (0 <= lo and lo <= hi and hi < max_bits):
      raise Exception('BitField %s: range illegal' % repr(self))

  def num_bits(self):
    """"returns the number of bits represented by the bitfield."""
    return self._hi - self._lo + 1

  def mask(self):
    mask = 0
    for i in range(1, self.num_bits()):
      mask = (mask << 1) + 1
    mask = mask << self._lo
    return mask

  def name(self):
    return self._name

  def hi(self):
    return self._hi

  def lo(self):
    return self._lo

  def to_bitfield(self, option={}):
    return self

  def to_uint32(self, options={}):
    masked_value ="(%s & 0x%08X)" % (self._name.to_uint32(options), self.mask())
    if self._lo != 0:
      masked_value = '(%s >> %s)' % (masked_value, self._lo)
    return masked_value

  def _max_bits(self, options):
    """Returns the maximum number of bits allowed."""
    max_bits = options.get('max_bits')
    if not max_bits:
      max_bits = 32
    return max_bits

  def __repr__(self):
    return self._named_repr(repr(self._name))

  def neutral_repr(self):
    return self._named_repr(self._name.neutral_repr())

  def _named_repr(self, name):
    if self._hi == self._lo:
      return '%s(%s)' % (name, self._hi)
    else:
      return '%s(%s:%s)' % (name, self._hi, self._lo)

class Instruction(BitExpr):
  """Models references to the intruction being decoded."""

  def to_uint32(self, options={}):
    return '%s.Bits()' % self._inst_name(options)

  def __repr__(self):
    return self._inst_name()

  def neutral_repr(self):
    return self._inst_name()

  def _inst_name(self, options={}):
    inst = options.get('inst')
    if not inst:
      inst = 'inst'
    return inst

class SafetyAction(BitExpr):
  """Models a safety check, and the safety action returned."""

  def __init__(self, test, action):
    # Note: The following list is from inst_classes.h, and should
    # be kept in sync with that file (see type SafetyLevel).
    if action not in ['UNKNOWN', 'UNDEFINED', 'NOT_IMPLEMENTED',
                      'UNPREDICTABLE', 'DEPRECATED', 'FORBIDDEN',
                      'FORBIDDEN_OPERANDS', 'MAY_BE_SAFE']:
      raise Exception("Safety action %s => %s not understood" %
                      (test, action))
    self._test = test
    self._action = action

  def test(self):
    return self._test

  def action(self):
    return self._action

  def to_bitfield(self, options={}):
    raise Exception("to_bitfield not defined for %s" % self)

  def to_bool(self, options={}):
    # Be sure to handle inflection of safety value when defining the boolean
    # value the safety action corresponds to.
    if self._action == 'MAY_BE_SAFE':
      return self._test.to_bool(options)
    else:
      return self._test.negate().to_bool(options)

  def to_register(self, options={}):
    raise Exception("to_register not defined for %s" % self)

  def to_register_list(self, options={}):
    raise Exception("to_register_list not defined for %s" % self)

  def to_uint32(self, options={}):
    raise Exception("to_uint32 not defined for %s" % self)

  def __repr__(self):
    return '%s => %s' % (repr(self._test), self._action)

  def neutral_repr(self):
    return '%s => %s' % (neutral_repr(self._test), self._action)

class SymbolTable(object):
  """Holds mapping from names to corresponding value."""

  def __init__(self):
    self.dict = {}

  def copy(self):
    """Returns a copy of the symbol table"""
    st = SymbolTable()
    for k in self.dict:
      st.dict[k] = self.dict[k]
    return st

  def find(self, name):
    value = self.dict.get(name)
    if value: return value
    inherits = self.dict.get('$inherits$')
    if not inherits: return None
    return inherits.find(name)

  def define(self, name, value, fail_if_defined = True):
    """Adds (name, value) pair to symbol table if not already defined.
       Returns True if added, otherwise False.
       """
    if self.dict.get(name):
      if fail_if_defined:
        raise Exception('%s: multiple definitions' % name)
      return False
    else:
      self.dict[name] = value
      return True

  def remove(self, name):
    self.dict.pop(name)

  def inherits(self, context):
    """Adds inheriting symbol table."""
    self.define('$inherits$', context)

  def disinherit(self):
    """Removes inheriting symbol table."""
    self.remove('$inherits$')

  def keys(self):
    return [k for k in self.dict.keys() if k != '$inherits$']

  def __hash__(self):
    return hash(self.neutral_repr())

  def __cmp__(self, other):
    if isinstance(other, SymbolTable) or isinstance(other, BitExpr):
      return cmp(self.neutral_repr(), other.neutral_repr())
    else:
      value = cmp(type(self), type(other))
      if value != 0: return value
      return cmp(self.dict, other.dict)

  def neutral_repr(self):
    neutral_dict = {}
    for k in self.dict.keys():
      key = neutral_repr(k)
      value = neutral_repr(self.dict[k])
      neutral_dict[key] = value
    return self._describe(neutral_dict)

  def __repr__(self):
    return self._describe(self.dict)

  def _describe(self, dict):
    dict_rep = '{'
    is_first = True
    for k in sorted(dict.keys()):
      if is_first:
        is_first = False
      else:
        dict_rep = '%s,%s  ' % (dict_rep, NEWLINE_STR)
      dict_rep = "%s%s: %s" % (dict_rep, k, dict[k])
    dict_rep += '}'
    return dict_rep

class BitPattern(BitExpr):
    """A pattern for matching strings of bits.  See parse() for
       syntax."""

    @staticmethod
    def parse(pattern, column):
        """Parses a string pattern describing some bits.  The string
           can consist of '1' and '0' to match bits explicitly, 'x' or
           'X' to ignore bits, '_' as an ignored separator, and an
           optional leading '~' to negate the entire pattern.
           Examples:

             10xx0
             1111_xxxx
             ~111x

        The pattern may also optionally be '-', which is equivalent to
        a sequence of 'xxx...xxx' of the requested width.

        Args:
            pattern: a string in the format described above.
            column: The tuple (name, hi, lo) defining a column.
        Returns:
            A BitPattern instance that describes the match, and is capable of
            transforming itself to a C expression.
        Raises:
            Exception: the input didn't meet the rules described above.
        """
        col = column.to_bitfield()
        hi_bit = col.hi()
        lo_bit = col.lo()
        num_bits = col.num_bits()
        # Convert - into a full-width don't-care pattern.
        if pattern == '-':
            return BitPattern.parse('x' * num_bits, column)

        # Derive the operation type from the presence of a leading
        # tilde.
        if pattern.startswith('~'):
            op = '!='
            pattern = pattern[1:]
        else:
            op = '=='

        # Allow use of underscores anywhere in the pattern, as a
        # separator.
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
            elif c.isalpha():  # covers both rule patterns and table patterns
                mask = mask << 1
                value = value << 1
            else:
                raise Exception('Invalid characters in pattern %s' % pattern)

        mask = mask << lo_bit
        value = value << lo_bit
        return BitPattern(mask, value, op, col)

    @staticmethod
    def parse_catch(pattern, column):
        """"Calls parse with given arguments, and catches exceptions
            raised. Prints raised exceptions and returns None.
        """
        try:
            return BitPattern.parse(pattern, column);
        except Exception as ex:
            print "Error: %s" % ex
            return None

    @staticmethod
    def always_matches(column=None):
      """Returns a bit pattern corresponding to always matches."""
      return BitPattern(0, 0, '==', column)

    def matches_any(self):
      """Returns true if pattern matches any pattern of bits."""
      return self.mask == 0

    def negate(self):
      """Returns pattern that is negation of given pattern"""
      if self.is_equal_op():
        return BitPattern(self.mask, self.value, '!=', self.column)
      else:
        return BitPattern(self.mask, self.value, '==', self.column)

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
        self.column = column

        # Fail if we get something we don't know how to handle.
        if column:
          Good = isinstance(column, BitField)
          if isinstance(column, IdRef):
            Good = isinstance(column.value, BitField)
          if not Good:
            raise Exception(
                "Don't know how to generate bit pattern for %s" % column)

    def signif_bits(self):
      """Returns the number of signifcant bits in the pattern
         (i.e. occurrences of 0/1 in the pattern."""
      return _popcount(self.mask)

    def copy(self):
      """Returns a copy of the given bit pattern."""
      return BitPattern(self.mask, self.value, self.op, self.column)

    def union_mask_and_value(self, other):
      """Returns a new bit pattern unioning the mask and value of the
         other bit pattern."""
      return BitPattern(self.mask | other.mask, self.value | other.value,
                        self.op, self.column)

    def is_equal_op(self):
      """Returns true if the bit pattern is an equals (rather than a
         not equals)."""
      return self.op == '=='

    def conflicts(self, other):
        """Returns an integer with a 1 in each bit position that
           conflicts between the two patterns, and 0s elsewhere.  Note
           that this is only useful if the masks and ops match.
        """
        return (self.mask & self.value) ^ (other.mask & other.value)

    def is_complement(self, other):
        """Checks if two patterns are complements of each other.  This
           means they have the same mask and pattern bits, but one is
           negative.
        """
        return (self.op != other.op
            and self.mask == other.mask
            and self.value == other.value)

    def strictly_overlaps(self, other):
      """Checks if patterns overlap, and aren't equal."""
      return ((self.mask & other.mask) != 0) and (self != other)

    def is_strictly_compatible(self, other):
        """Checks if two patterns are safe to merge using +, but are
           not ==."""
        if self.is_complement(other):
            return True
        elif self.op == other.op:
            return (self.mask == other.mask
                and _popcount(self.conflicts(other)) == 1)
        return False

    def categorize_match(self, pattern):
      """ Compares this pattern againts the given pattern, and returns one
          of the following values:

          'match' - All specified bits in this match the corresponding bits in
                  the given pattern.
          'conflicts' - There are bits in this pattern that conflict with the
                  given pattern. Hence, there is no way this pattern will
                  succeed for instructions matching the given pattern.
          'consistent' - The specified bits in this pattern neither match,
                  nor conflicts with the unmatched pattern. No conclusions
                  can be drawn from the overlapping bits of this and the
                  given pattern.
          """
      if self.is_equal_op():
        # Compute the significant bits that overlap between this pattern and
        # the given pattern.
        mask = (self.mask & pattern.mask)
        if pattern.is_equal_op():
          # Testing if significant bits of this pattern differ (i.e. conflict)
          # with the given pattern.
          if mask & (self.value ^ pattern.value):
            # Conflicts, no pattern match.
            return 'conflicts'
          else:
            # Matches on signifcant bits in mask
            return 'match'
        else:
          # Test if negated given pattern matches the significant
          # bits of this pattern.
          if mask & (self.value ^ ~pattern.value):
            # Conflicts, so given pattern can't match negation. Hence,
            # this pattern succeeds.
            return 'match'
          else:
            # Consistent with negation.
            if mask == pattern.mask:
              # Matched all bits. pattern matches.
              return 'match'
            else:
              # Only some bits matched. Hence, we can draw no conclusions other
              # than consistent.
              return 'consistent'
      else:
        # self match on negation.
        negated_self = self.copy()
        negated_self.op = '=='
        result = negated_self.categorize_match(pattern)
        if result == 'match':
          return 'match'
        else:
          # Not exact match. Can only assume they are consistent (since none
          # of the bits conflicted).
          return 'consistent'

    def remove_overlapping_bits(self, pattern):
      """Returns a copy of this with overlapping significant bits of this
         and the given pattern.
      """
      # Compute significant bits that overlap between this pattern and
      # the given pattern, and build a mask to remove those bits.
      mask = ~(self.mask & pattern.mask)

      # Now build a new bit pattern with overlapping bits removed.
      return BitPattern((mask & self.mask),
                        (mask & self.value),
                        self.op,
                        self.column)

    def __add__(self, other):
        """Merges two compatible patterns into a single pattern that matches
        everything either pattern would have matched.
        """
        assert (self == other) or self.is_strictly_compatible(other)

        if self.op == other.op:
            c = self.conflicts(other)
            return BitPattern((self.mask | other.mask) ^ c,
                (self.value | other.value) ^ c, self.op, self.column)
        else:
            return BitPattern.always_matches(self.column)

    def to_bool(self, options={}):
      # Generate expression corresponding to bit pattern.
      if self.mask == 0:
        value = 'true'
      else:
        inst = self.column.name().to_uint32(options)
        value = ('(%s & 0x%08X) %s 0x%08X'
                 % (inst, self.mask, self.op, self.value))
      return value

    def to_commented_bool(self, options={}):
      if not self.column and self.mask == 0:
        # No information is provided by the comment, so don't add!
        return 'true'
      return BitExpr.to_commented_bool(self, options)

    def bitstring(self):
      """Returns a string describing the bitstring of the pattern."""
      bits = self._bits_repr()
      if self.column:
        col = self.column.to_bitfield()
        bits = bits[col.lo() : col.hi() + 1]
      bits.reverse()
      return ''.join(bits)

    def __hash__(self):
       value = hash(self.mask) + hash(self.value) + hash(self.op)
       if self.column:
         value += hash(neutral_repr(self.column))
       return value

    def __cmp__(self, other):
        """Compares two patterns for sorting purposes.  We sort by
        - # of significant bits, DESCENDING,
        - then mask value, numerically,
        - then value, numerically,
        - and finally op.

        This is also used for equality comparison using ==.
        """
        return (cmp(other.signif_bits(), self.signif_bits())
            or cmp(self.mask, other.mask)
            or cmp(self.value, other.value)
            or cmp(self.op, other.op)
            or cmp(neutral_repr(self.column), neutral_repr(other.column)))

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
      for c in columns:
        hi_bit = c.hi()
        lo_bit = c.lo()
        index = self.first_bit()
        if index is None : continue
        if index >= lo_bit and index <= hi_bit:
          return BitPattern(self.mask, self.value, self.op, c)
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

    def neutral_repr(self):
        if self.column:
          return '%s=%s%s' % (self.column.neutral_repr(),
                              '' if self.is_equal_op() else'~',
                              self.bitstring())
        elif self.is_equal_op():
          return self.bitstring()
        else:
          return "~%s" % self.bitstring()


    def __repr__(self):
        """Returns the printable string for the bit pattern."""
        if self.column:
          return '%s=%s%s' % (self.column,
                              '' if self.is_equal_op() else'~',
                              self.bitstring())
        elif self.is_equal_op():
          return self.bitstring()
        else:
          return "~%s" % self.bitstring()

class RuleRestrictions(object):
  """A rule restriction defines zero or more (anded) bit patterns, and
     an optional other (i.e. base class) restriction to be used when testing.
     """

  def __init__(self, restrictions=[], other=None):
    self.restrictions = restrictions[:]
    self.other = other

  def __hash__(self):
    return sum([hash(r) for r in self.restrictions]) + hash(self.other)

  def IsEmpty(self):
    return not self.restrictions and not self.other

  def add(self, restriction):
    self.restrictions = self.restrictions + [restriction]

  def __repr__(self):
    """ Returns the printable string for the restrictions. """
    rep = ''
    if self.restrictions:
      for r in self.restrictions:
        rep += '& ' + r
      rep += ' '
    if self.other:
      rep += ('& other: %s' % self.other)
    return rep

  def __cmp__(self, v):
    value = cmp(self.other, v.other)
    if value != 0: return value
    return cmp(self.restrictions, v.restrictions)

TABLE_FORMAT="""
Table %s
%s
%s
"""
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

    def add_column(self, column):
        """Adds a column to the table. Returns true if successful."""
        for col in self._columns:
          if repr(col) == repr(column):
            return False
        self._columns.append(column)
        return True

    def rows(self, default_also = True):
        """Returns all rows in table (including the default row
           as the last element if requested).
        """
        r = self._rows[:]
        if default_also and self.default_row:
          r.append(self.default_row)
        return r

    def add_default_row(self, action):
        """Adds a default action to use if none of the rows apply.
           Returns True if able to define.
        """
        if self.default_row: return False
        self.default_row = Row([BitPattern.always_matches()], action)
        return True

    def add_row(self, patterns, action):
        """Adds a row to the table.
        Args:
            patterns: a list containing a BitPattern for every column in the
                table.
            action: The action associated  with the row. Must either be
                a DecoderAction or a DecoderMethod.
        """
        row = Row(patterns, action)
        self._rows.append(row)
        return row

    def remove_table(self, name):
      """Removes method calls to the given table name from the table"""
      for row in self._rows:
        row.remove_table(name)

    def define_pattern(self, pattern, column):
        """Converts the given input pattern (for the given column) to the
           internal form. Returns None if pattern is bad.
        """
        if column >= len(self._columns): return None
        return BitPattern.parse_catch(pattern, self._columns[column])

    def action_filter(self, names):
        """Returns a table with DecoderActions reduced to the given field names.
           Used to optimize out duplicates, depending on context.
        """
        table = Table(self.name, self.citation)
        table._columns = self._columns
        for r in self._rows:
          table.add_row(r.patterns, r.action.action_filter(names))
        if self.default_row:
          table.add_default_row(self.default_row.action.action_filter(names))
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
        new_rows.append(Row(new_patterns, r.action))
      return new_rows

    def methods(self):
      """Returns the (sorted) list of methods called by the table."""
      methods = set()
      for r in self.rows(True):
        if r.action.__class__.__name__ == 'DecoderMethod':
          methods.add(r.action)
      return sorted(methods)

    def __repr__(self):
      return TABLE_FORMAT % (self.name,
                             ' '.join([repr(c) for c in self._columns]),
                             NEWLINE_STR.join([repr(r) for r in self._rows]))

class DecoderAction:
  """An action defining a class decoder to apply.
     Fields are:
       baseline - Name of class decoder used in the baseline.
       actual - Name of the class decoder to use in the validator.
       st - Symbol table of other information stored on the decoder action.
  """
  def __init__(self, baseline, actual):
    self.baseline = baseline
    self.actual = actual
    self.st = SymbolTable()
    self.st.dict['constraints'] = RuleRestrictions()

  def action_filter(self, names):
    action = DecoderAction(
        self.baseline if 'baseline' in names else None,
        self.actual if ('actual' in names or
                        ('actual-not-baseline' in names and
                         self.actual != self.baseline)) else None)
    for n in names:
      value = self.st.dict.get(n)
      if value:
        action.st.dict[n] = value
    return action

  def pattern(self):
    """Returns the pattern associated with the action."""
    return self.st.dict.get('pattern')

  def rule(self):
    """Returns the rule associated with the action."""
    return self.st.dict.get('rule')

  def constraints(self):
    """Returns the pattern restrictions associated with the action."""
    return self.st.dict.get('constraints')

  def safety(self):
    """Returns the safety associated with the action."""
    s = self.st.dict.get('safety')
    return s if s else []

  def __eq__(self, other):
    return (isinstance(other, DecoderAction)
            and self.baseline == other.baseline
            and self.actual == other.actual
            and self.st == other.st)

  def __cmp__(self, other):
    # Order lexicographically on type/fields.
    value = cmp(type(self), type(other))
    if value != 0: return value
    value = cmp(self.baseline, other.baseline)
    if value != 0: return value
    value = cmp(self.actual, other.actual)
    if value != 0: return value
    return cmp(self.st, other.st)

  def __hash__(self):
    return (hash('DecoderAction') + hash(self.baseline) +
            hash(self.actual) + hash(self.st))

  def __repr__(self):
    class_name = (("%s => %s" % (self.baseline, self.actual))
                   if self.actual else self.baseline)
    return '= %s %s' % (class_name, repr(self.st))

  def neutral_repr(self):
    class_name = (("%s => %s" % (self.baseline, self.actual))
                   if self.actual else self.baseline)
    return '= %s %s' % (class_name, self.st.neutral_repr())

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
    return '-> %s' % self.name

class Row(object):
    """ A row in a Table."""
    def __init__(self, patterns, action):
        """Initializes a Row.
        Args:
            patterns: a list of BitPatterns that must match for this Row to
                match.
            action: the action to be taken if this Row matches.
        """
        self.patterns = patterns
        self.action = action

        self.significant_bits = 0
        for p in patterns:
            self.significant_bits += p.signif_bits()

    def add_pattern(self, pattern):
        """Adds a pattern to the row."""
        self.patterns.append(pattern)

    def remove_table(self, name):
      """Removes method call to the given table name from the row,
         if applicable.
         """
      if (isinstance(self.action, DecoderMethod) and
          self.action.name == name):
        self.action = DecoderAction('NotImplemented', 'NotImplemented')

    def strictly_overlaps_bits(self, bitpat):
      """Checks if bitpat strictly overlaps a bit pattern in the row."""
      for p in self.patterns:
        if bitpat.strictly_overlaps(p):
          return True
      return False

    def can_merge(self, other):
        """Determines if we can merge two Rows."""
        if self.action != other.action:
            return False

        equal_columns = 0
        compat_columns = 0
        for (a, b) in zip(self.patterns, other.patterns):
            if a == b:
                equal_columns += 1
            # Be sure the column doesn't overlap with other columns in pattern.
            if (not self.strictly_overlaps_bits(a) and
                not other.strictly_overlaps_bits(b) and
                a.is_strictly_compatible(b)):
                compat_columns += 1

        cols = len(self.patterns)
        return (equal_columns == cols
            or (equal_columns == cols - 1 and compat_columns == 1))

    def copy_with_action(self, action):
      return Row(self.patterns, action)

    def copy_with_patterns(self, patterns):
      return Row(patterns, self.action)

    def __add__(self, other):
        assert self.can_merge(other)  # Caller is expected to check!
        return Row([a + b for (a, b) in zip(self.patterns, other.patterns)],
            self.action)

    def __cmp__(self, other):
        """Compares two rows, so we can order pattern matches by specificity.
        """
        return (cmp(self.patterns, other.patterns)
            or cmp(self.action, other.action))

    def __repr__(self):
      return self._describe([repr(p) for p in self.patterns], repr(self.action))

    def neutral_repr(self):
      return self._describe([neutral_repr(p) for p in self.patterns],
                            neutral_repr(self.action))

    def _describe(self, patterns, action):
        return (ROW_PATTERN_ACTION_FORMAT %
                (' & '.join(patterns), action.replace(NEWLINE_STR,
                                                      ROW_ACTION_INDENT)))

ROW_PATTERN_ACTION_FORMAT="""%s
    %s"""

ROW_ACTION_INDENT="""
   """

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

  def get_table(self, name):
    """Returns the table with the given name"""
    for tbl in self._tables:
      if tbl.name == name:
        return tbl
    return None

  def remove_table(self, name):
    """Removes the given table from the decoder"""
    new_tables = []
    for table in self._tables:
      if table.name != name:
        new_tables = new_tables + [table]
        table.remove_table(name)
    self._tables = new_tables

  def get_class_defs(self):
    return self._class_defs

  def set_class_defs(self, class_defs):
    self._class_defs = class_defs

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
            if isinstance(r.action, DecoderAction):
              decoders.add(r.action)
    return sorted(decoders)

  def rules(self):
    """Returns the sorted sequence of DecoderActions that define
       the rule field.
    """
    return sorted(filter(lambda (r): r.rule, self.decoders()))

  def show_table(self, table):
    tbl = self.get_table(table)
    if tbl != None:
      print "%s" % tbl
    else:
      raise Exception("Can't find table %s" % table)
