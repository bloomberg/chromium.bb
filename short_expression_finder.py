# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

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
  """

  # These must be positive integers.
  COMPARISON_COST = 1
  PAREN_COST = 1

  def __init__(self, variables_and_values):
    assert variables_and_values
    for k, vs in variables_and_values:
      assert re.match(r'\w+\Z', k)
      assert vs
      assert (all(isinstance(v, int) for v in vs) or
          all(re.match(r'\w+\Z', v) for v in vs))

    variables, values = zip(*((k, sorted(v)) for k, v in variables_and_values))
    valuecounts = map(len, values)
    base_tests_by_cost = {}

    # Loop over nonempty subsets of values of each variable. This is about 2^n
    # cases where n is the total number of values (currently 2+3=5 in Chrome).
    for subsets in itertools.product(*(range(1, 1 << n) for n in valuecounts)):
      # Supposing values == [['a', 'b', 'c'], [1, 2]], there are six
      # configurations: ('a', 1), ('a', 2), ('b', 1), etc. Each gets a bit, in
      # that order starting from the LSB. Start with the equivalent of
      # set([('a', 1)]) and massage that into the correct set of configs.
      bits = 1
      shift = 1
      cost = 0
      for subset, valuecount in zip(subsets, valuecounts):
        oldbits, bits = bits, 0
        while subset:
          if subset & 1:
            bits |= oldbits
            cost += self.COMPARISON_COST
          oldbits <<= shift
          subset >>= 1
        shift *= valuecount
      # Charge an extra set of parens for the whole expression,
      # which is removed later if appropriate.
      cost += self.PAREN_COST * (1 + sum(bool(n & (n-1)) for n in subsets))
      base_tests_by_cost.setdefault(cost, {})[bits] = subsets

    self.variables = variables
    self.values = values
    self.base_tests_by_cost = base_tests_by_cost

  def get_expr(self, configs):
    assert configs
    for config in configs:
      assert len(config) == len(self.values)
      assert all(val in vals for val, vals in zip(config, self.values))
    return self._format_expr(self._get_expr_internal(configs))

  def _get_expr_internal(self, configs):
    bits = 0
    for config in configs:
      bit = 1
      n = 1
      for value, values in zip(config, self.values):
        bit <<= (n * values.index(value))
        n *= len(values)
      bits |= bit
    notbits = ~bits

    def try_partitions(parts, bits):
      for cost, subparts in parts:
        if cost is None:
          return None if bits else ()
        try:
          tests = self.base_tests_by_cost[cost]
        except KeyError:
          continue
        for test in tests:
          if (test & bits) and not (test & notbits):
            result = try_partitions(subparts, bits & ~test)
            if result is not None:
              return (tests[test],) + result
      return None

    for total_cost in itertools.count(0):
      try:
        return (self.base_tests_by_cost[total_cost + self.PAREN_COST][bits],)
      except KeyError:
        result = try_partitions(tuple(partitions(total_cost, self.PAREN_COST)),
                                bits)
        if result is not None:
          return result

  def _format_expr(self, expr):
    out = []
    for expr2 in expr:
      out2 = []
      for name, values, expr3 in zip(self.variables, self.values, expr2):
        out3 = []
        for value in values:
          if expr3 & 1:
            if isinstance(value, basestring):
              value = '"%s"' % value
            out3.append('%s==%s' % (name, value))
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
  in a slightly odd tree format. Most of the tree is left unevaluated.
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
