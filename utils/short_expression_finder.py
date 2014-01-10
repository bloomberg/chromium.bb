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
          all(re.match(r'^\w+$', v) for v in vs)), vs

    # Converts to independent arrays, first for the keys, second for the values
    # for each key. Sort the valid values so they can be output ordered
    # naturally, e.g. 'OS=="linux" or OS=="win"' and not
    # 'OS=="win" or OS=="linux"' which would make everyone sad!
    self.variables, self.values = zip(
        *((k, sorted(vs)) for k, vs in variables_and_values))
    # The cost mapping on conditions.
    self.base_tests_by_cost = _calculate_costs(
        self.values, self.COMPARISON_COST, self.PAREN_COST)

  def get_expr(self, configs):
    """Returns a GYP condition string for a list of config values."""
    assert configs
    assert isinstance(configs, (tuple, list, frozenset)), configs
    for config in configs:
      assert config
      assert isinstance(config, tuple), config
      assert len(config) == len(self.values), (config, self.values)
      assert all(val in vals for val, vals in zip(config, self.values)), (
          config, self.values)
    return self._format_expr(self._get_expr_internal(configs))

  def _get_expr_internal(self, configs):
    """Returns a GYP condition expression for a list of config values.

    The return value needs to passed to _format_expr() to be usable.
    """
    assert configs
    assert isinstance(configs, (tuple, list, frozenset))
    # Evaluate the cost to find the lowest cost in the condition.
    bits = 0
    for config in configs:
      assert config
      assert isinstance(config, tuple)
      bit = 1
      n = 1
      for value, values in zip(config, self.values):
        bit <<= (n * values.index(value))
        n *= len(values)
      bits |= bit
    notbits = ~bits

    # Find the absolute lowest cost, e.g. the lesser number of conditions to fit
    # the whole variables-dimensions values matrix. N-dimensions where
    # N==len(variables).
    for total_cost in itertools.count(0):
      try:
        return (self.base_tests_by_cost[total_cost + self.PAREN_COST][bits],)
      except KeyError:
        result = _try_partitions(
            self.base_tests_by_cost,
            tuple(partitions(total_cost, self.PAREN_COST)),
            bits,
            notbits)
        if result is not None:
          return result

  def _format_expr(self, expr):
    """Returns a GYP condition string for an expression."""
    out = []
    for expr2 in expr:
      out2 = []
      assert isinstance(expr2, tuple)
      # Process 'and', e.g. the combination of all the bounded variables are
      # that required.
      for variable_name, valid_values, expr3 in zip(
          self.variables, self.values, expr2):
        assert isinstance(expr3, int)
        out3 = []
        # Process 'or', e.g. all the valid values for this specific variable.
        for value in valid_values:
          if expr3 & 1:
            # If it is a string, quote it. Otherwise it is unnecessary.
            if isinstance(value, basestring):
              value = '"%s"' % value
            out3.append('%s==%s' % (variable_name, value))
          expr3 >>= 1

        out2.append(' or '.join(out3))
        if len(out3) > 1 and len(expr2) > 1:
          out2[-1] = '(%s)' % out2[-1]

      out.append(' and '.join(out2))
      if len(out2) > 1 and len(expr) > 1:
        out[-1] = '(%s)' % out[-1]
    return ' or '.join(out)


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
  first time it is iterated over.
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
