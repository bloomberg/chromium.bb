# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Generates short, hopefully readable Python expressions that test variables
for certain combinations of values.
"""

import itertools
import re


class ShortExpressionFinder(object):
  """Usage:
    >>> sef = ShortExpressionFinder([('foo', ("a", "b", "c", "d")),
    ...                              ('bar', (1, 2))])
    >>> sef.get_expr([("a", 1)])
    foo=="a" and bar==1
    >>> sef.get_expr([("a", 1), ("b", 2), ("c", 1)])
    ((foo=="a" or foo=="c") and bar==1) or (foo=="b" and bar==2)

  The returned expressions are of the form
    EXPR ::= EXPR2 ( "or" EXPR2 )*
    EXPR2 ::= EXPR3 ( "and" EXPR3 )*
    EXPR3 ::= VAR "==" VALUE ( "or" VAR "==" VALUE )*
  where all of the comparisons in an EXPR2 involve the same variable.
  Only positive tests are used so that all expressions will evaluate false if
  given an unanticipated combination of values.

  A "cheapest" expression is returned. The cost of an expression is a function
  of the number of var==value comparisons and the number of parentheses.

  The expression is found by exhaustive search, but it seems to be adequately
  fast even for fairly large sets of configuration variables.

  This class is immutable.
  """

  # These must be positive integers.
  COMPARISON_COST = 1
  PAREN_COST = 1

  def __init__(self, variables_and_values):
    # Ensures variables_and_values is a list of tuples.
    # TODO(maruel): Why not a dict? Variable names must be unique and there is
    # no inherent ordering in the logic here.
    assert variables_and_values
    assert isinstance(variables_and_values, list)
    for item in variables_and_values:
      assert isinstance(item, tuple)
      assert len(item) == 2
      k, vs = item
      assert isinstance(k, basestring)
      assert re.match(r'\w+\Z', k)
      assert vs
      # TODO(maruel): Be stricter about the values types.
      assert isinstance(vs, (list, tuple, set, frozenset))
      # The values are either all numbers or all strings.
      assert (
          all(isinstance(v, int) for v in vs) or
          all(re.match(r'\w+\Z', v) for v in vs))

    # Converts to independent arrays, first for the keys, second for the values
    # for each key. Sort the valid values so they can be output ordered
    # naturally, e.g. 'OS=="linux" or OS=="win"' and not
    # 'OS=="win" or OS=="linux"' which would make everyone sad!
    self.variables, self.values = zip(
        *((k, sorted(vs)) for k, vs in variables_and_values))

  def get_expr(self, conditions):
    """Returns a GYP condition string for a list of values to match.

    The work here is figuring out the intersecting conditons that can be reduced
    and optimize the output accordingly. In addition, conditions with a free
    variable must be taken care of explicitly.
    """
    assert conditions
    assert isinstance(conditions, (tuple, list, frozenset)), conditions
    for condition in conditions:
      assert condition
      assert isinstance(condition, tuple), condition
      assert len(condition) == len(self.values), (condition, self.values)
      assert all(
          val in vals or val is None
          for val, vals in zip(condition, self.values)), (
              condition, self.values)

    # The first thing is to isolate the conditions with free variable(s) into
    # buckets. A value set to None means it is free. This makes it much simpler
    # to reduce the conditions afterward; it makes it possible to differentiate
    # between an unbounded value or if all the known values must be tested.
    #
    # For example, if chromeos can be 0 or 1, it differentiate between: [(None,
    # 'linux')] vs [(0, 'linux'), (1, 'linux')].  Converts 'conditions' into a
    # bitarray.
    buckets = {}
    for condition in conditions:
      key = tuple(v is not None for v in condition)
      buckets.setdefault(key, []).append(condition)

    # Then process per bound variables buckets and 'or' the whole thing at the
    # end.
    out = []
    for bound, subconditions in buckets.iteritems():
      values = [v for b, v in zip(bound, self.values) if b]
      variables = [v for b, v in zip(bound, self.variables) if b]
      # Strip out unbounded variable.
      subconditions = [
          tuple(x for x in c if x is not None) for c in subconditions
      ]
      bits = _get_expr(values, subconditions)

      # Converts the bitarray into a list of reduced conditions that matches the
      # matrix.
      assertions = _find_reduced_cost(
          self.COMPARISON_COST, self.PAREN_COST, values, bits)
      # Render the list of bitarrays.
      out.append(_format_expr(variables, values, assertions))

    result = []
    for s, n in out:
      if n > 1 and len(out) > 1:
        # There is multiple conditions, so parenthesis must be added.
        s = '(%s)' % s
      result.append(s)
    return ' or '.join(result)


def partitions(n, minimum):
  """Yields all the ways of expressing n as a sum of integers >= minimum,
  in a slightly odd tree format.

  Most of the tree is left unevaluated.
  Example:
    partitions(4, 1) ==>
       [1, <[1, <[1, <[1, <end>]>],
                 [2, <end>]>],
            [3, <end>]>],
       [2, <[2, <end>]>],
       [4, <end>]
  where <...> is a lazily-evaluated list and end == [None, None].
  """
  if n == 0:
    yield (None, None)
  for k in range(n, minimum - 1, -1):
    children = partitions(n - k, k)
    # We could just yield [k, children] here, but that would create a lot of
    # blind alleys with no actual partitions.
    try:
      yield [k, MemoizedIterable(itertools.chain([next(children)], children))]
    except StopIteration:
      pass


class MemoizedIterable(object):
  """Wrapper for an iterable that fully evaluates and caches the values the
  first time it is iterated over, but not before.
  """
  def __init__(self, iterable):
    self.iterable = iterable
  def __iter__(self):
    self.iterable = tuple(self.iterable)
    return iter(self.iterable)


def _calculate_costs(values, comparison_cost, paren_cost):
  """Returns the weight costs for this mapping table.

  The values themselves are not used, only the number of possibility (count of
  valid values per set) by variable is used.

  Supposing values == [['a', 'b', 'c'], [1, 2]], there are six combinations:
  ('a', 1), ('a', 2), ('b', 1), ('b', 2), ('b', 3).

  This is about 2^n cases where n is the total number of values.
  """
  base_tests_by_cost = {}
  valuecounts = map(len, values)
  for subsets in itertools.product(*(range(1, 1 << n) for n in valuecounts)):
    # Each combination gets a bit, in that order starting from the LSB. Start
    # with the equivalent of set([('a', 1)]) (from the example in the docstring)
    # and massage that into the correct set of variables.
    bits = 1
    shift = 1
    cost = 0
    for subset, valuecount in zip(subsets, valuecounts):
      if valuecount:
        oldbits, bits = bits, 0
        while subset:
          if subset & 1:
            bits |= oldbits
            cost += comparison_cost
          oldbits <<= shift
          subset >>= 1
      shift *= valuecount
    # Charge an extra set of parens for the whole expression, which is removed
    # later if appropriate.
    cost += paren_cost * (1 + sum(bool(n & (n-1)) for n in subsets))
    base_tests_by_cost.setdefault(cost, {})[bits] = subsets
  return base_tests_by_cost


def _get_expr(all_values, conditions):
  """Calculates a bitarray for a list of bounded values.

  Arguments:
    all_values: a list of lists of possible values for each variable. All
                variables must be bound.
    conditions: the matrix of bound values to create a bitarray for.

  Return:
    flattened bitset of all the conditions that must be True. It is a very
    compact representation of conditions. All the conditions are or'ed together.
    This value has 1 bit set per *unique* condition met. The largest bit that
    can be set is reduce(lambda x, y: x*y, map(len(values)).

  This removes all the references to the actual variables values to do a pure
  optimization phase.

  The return value needs to passed to _find_reduced_cost() to be reduced and
  then rendered with _format_expr() to be human readable.

  See unit test for examples and die a little inside. Then tell yourself that it
  works and there's people smarter than you on Earth.
  """
  assert conditions
  assert isinstance(conditions, (tuple, list, frozenset))

  bits = 0L
  for condition in conditions:
    bits |= _get_bit(all_values, condition)
  return bits


def _get_bit(all_values, condition):
  """Get the bit(s) for this specific condition.

  A single bit is returned when all variables are specified. Unbounded variable
  is not supported and must be filtered out prior to calling this function.
  """
  assert isinstance(condition, tuple)
  assert len(condition) == len(all_values), (condition, all_values)
  bit = 1L
  n = 1
  for value, values in zip(condition, all_values):
    assert value is not None
    # Move the dot.
    bit <<= (n * values.index(value))
    n *= len(values)
  return bit


def _find_reduced_cost(comparison_cost, paren_cost, all_values, bits):
  """Calculate the cost based on the variables conditions set in 'bits'.

  Arguments:
    comparison_cost: relative cost for a comparison.
    paren_cost: relative cost for parenthesis. Both value can be modulated to
                favor more comparisons or more parenthesis.
    all_values: list of lists of possible values for each variable. The values
                themselves are not used, only the number of elements in each
                variable.
    bits: a bitset. Each bit represents a valid condition that must be set.
          Multiple bits can be set, resulting in multiple conditions using the
          keyword 'or'.
  """
  base_tests_by_cost = _calculate_costs(
      all_values, comparison_cost, paren_cost)

  # Find the absolute lowest cost, e.g. the lesser number of conditions to fit
  # the whole variables-dimensions all_values matrix. N-dimensions where
  # N==len(variables).
  for total_cost in itertools.count(0):
    try:
      return (base_tests_by_cost[total_cost + paren_cost][bits],)
    except KeyError:
      result = _try_partitions(
          base_tests_by_cost,
          tuple(partitions(total_cost, paren_cost)),
          bits,
          ~bits)
      if result is not None:
        return result


def _try_partitions(base_tests_by_cost, parts, bits, notbits):
  """Tries to find the lowest cost.

  TODO(maruel): Document once I understand what it does.

  Arguments:
    base_tests_by_cost: dict as returned by _calculate_costs().
  """
  for cost, subparts in parts:
    if cost is None:
      return None if bits else ()

    try:
      tests = base_tests_by_cost[cost]
    except KeyError:
      continue

    for test in tests:
      if (test & bits) and not (test & notbits):
        result = _try_partitions(
            base_tests_by_cost, subparts, bits & ~test, notbits)
        if result is not None:
          return (tests[test],) + result


def _format_or(variable_name, valid_values, bitarray):
  """Formats the 'or' for all the valid_values with the corresponding bits set
  in bitarray.

  See unit test for examples.
  """
  assert isinstance(bitarray, int)
  out = []
  # Process 'or', e.g. all the valid values for this specific variable.
  for value in valid_values:
    if bitarray & 1:
      # If it is a string, quote it. Otherwise it is unnecessary.
      if isinstance(value, basestring):
        value = '"%s"' % value
      out.append('%s==%s' % (variable_name, value))
    bitarray >>= 1
  assert bitarray == 0, 'it is important to exhaust all the available options'
  return ' or '.join(out), len(out)


def _format_and(variables, values, bitarrays):
  """Format a condition string with all the bits sets in bitarrays.

  See unit test for examples.
  """
  assert len(variables) == len(bitarrays)
  conditions = []
  for variable_name, valid_values, bitarray in zip(
      variables, values, bitarrays):
    s, n = _format_or(variable_name, valid_values, bitarray)
    if n:
      conditions.append((s, n))

  out = []
  for s, n in conditions:
    if n > 1 and len(conditions) > 1:
      # There is multiple conditions, so parenthesis must be added.
      s = '(%s)' % s
    out.append(s)
  return ' and '.join(out), len(out)


def _format_expr(variables, values, valid_assertions):
  """Returns a GYP condition string for an set of valid bitarrays.

  See unit test for examples.
  For example, a equation may be true for both 'OS=="linux" and chromeos="0"' or
  'OS="win"'. This function handles the outher 'or'.

  valid_assertions may have a length of one.
  """
  conditions = []
  for bitarrays in valid_assertions:
    s, n = _format_and(variables, values, bitarrays)
    if n:
      conditions.append((s, n))

  out = []
  nb = len(conditions)
  for s, n in conditions:
    if n > 1:
      if len(conditions) > 1:
        # There is multiple conditions, so parenthesis must be added.
        s = '(%s)' % s
      else:
        # Surface the fact that no parenthesis was used.
        nb = n
    out.append(s)
  return ' or '.join(out), nb
