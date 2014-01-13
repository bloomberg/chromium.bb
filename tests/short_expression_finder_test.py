#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import itertools
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import short_expression_finder
from utils.short_expression_finder import ShortExpressionFinder, partitions


# Access to a protected member XXX of a client class
# pylint: disable=W0212


def matching_configs(expr, variables_and_values):
  """Return configs for which expr evaluates to true."""
  variables, values = zip(*variables_and_values)
  return tuple(
    config for config in itertools.product(*values)
    if eval(expr, dict(zip(variables, config)))
  )


class Tests(unittest.TestCase):
  def test_simple(self):
    sef = ShortExpressionFinder([('var', [17])])
    self.assertEqual('var==17', sef.get_expr([(17,)]))

  def test_two_variables_four_values(self):
    vals1, vals2 = (1, 2), ("ab", "cd")
    sef = ShortExpressionFinder([('var1', vals1), ('var2', vals2)])
    all_configs = tuple(itertools.product(vals1, vals2))
    for val1 in vals1:
      for val2 in vals2:
        self.assertEqual('var1==%d and var2=="%s"' % (val1, val2),
                         sef.get_expr([(val1, val2)]))
    for val1 in vals1:
      self.assertEqual('var1==%d and (var2=="ab" or var2=="cd")' % val1,
                       sef.get_expr([(val1, "ab"), (val1, "cd")]))
    for val2 in vals2:
      self.assertEqual('(var1==1 or var1==2) and var2=="%s"' % val2,
                       sef.get_expr([(1, val2), (2, val2)]))
    self.assertEqual('(var1==1 or var1==2) and (var2=="ab" or var2=="cd")',
                     sef.get_expr(all_configs))

  def check_expr(self, expr, variables_and_values):
    """Check that ShortExpressionFinder returns an expression that's logically
    equivalent to |expr| and equally simple, when given the matching configs.
    """
    configs = matching_configs(expr, variables_and_values)
    output_expr = ShortExpressionFinder(variables_and_values).get_expr(configs)
    output_configs = matching_configs(output_expr, variables_and_values)
    self.assertEqual(configs, output_configs)
    self.assertEqual(expr.count('=='), output_expr.count('=='))
    self.assertEqual(expr.count('('), output_expr.count('('))

  def test_single_nontrivial_region(self):
    self.check_expr('((OS=="linux" or OS=="mac" or OS=="win") and chromeos==0)'
                    ' or (OS=="linux" and chromeos==1)',
                    [('OS', 'linux mac win'.split()), ('chromeos', (0, 1))])

    self.check_expr('(OS=="linux" or OS=="mac" or OS=="win") and chromeos==0',
                    [('OS', 'linux mac win'.split()), ('chromeos', (0, 1))])

    self.check_expr('OS=="linux" and (chromeos==0 or chromeos==1)',
                    [('OS', 'linux mac win'.split()), ('chromeos', (0, 1))])

  def test_multiple_nontrivial_regions(self):
    # two disjoint regions of 1*2*1=2 and 2*1*2=4 configs, no overlap
    self.check_expr(
        '(p==2 and (q=="a" or q=="b") and r==10)'
        ' or ((p==1 or p==2) and q=="c" and (r==8 or r==10))',
        [('p', (1, 2, 3)), ('q', ('a', 'b', 'c')), ('r', (8, 9, 10))])

    # two regions of 4 and 8 configs with overlap
    self.check_expr(
        '((p==1 or p==2) and (q=="a" or q=="b") and r==9)'
        ' or ((p==2 or p==3) and q=="a" and (r==8 or r==9))',
        [('p', (1, 2, 3)), ('q', ('a', 'b', 'c')), ('r', (8, 9, 10))])

    # All configs except (p, q, r) == (2, 4, 6). There are simpler expressions
    # for this that don't fit ShortExpressionFinder's grammar.
    self.check_expr('((p==1 or p==2) and (q==3 or q==4) and r==5)'
                    ' or ((p==1 or p==2) and q==3 and r==6)'
                    ' or (p==1 and q==4 and r==6)',
                    [('p', (1, 2)), ('q', (3, 4)), ('r', (5, 6))])

  def test_100_variables(self):
    # This is kind of a cheat because the complexity is ((2^k)-1)^n for n
    # variables of k values each. With k=2 this would be hopelessly slow.
    self.check_expr(' and '.join('var%d=="val%d"' % (n, n) for n in range(100)),
                    [('var%d' % n, ('val%d' % n,)) for n in range(100)])

  def test_short_expression_finder_failure(self):
    # End users should never see these failures so it doesn't matter much how
    # exactly they fail.
    self.assertRaises(AssertionError, ShortExpressionFinder, [])
    self.assertRaises(AssertionError,
        ShortExpressionFinder, [('x', (1, 2)), ('no_values', ())])
    self.assertRaises(TypeError,
        ShortExpressionFinder, [('bad_type', (1, 2.5))])
    self.assertRaises(AssertionError,
        ShortExpressionFinder, [('bad name', (1, 2))])
    self.assertRaises(AssertionError,
        ShortExpressionFinder, [('x', ('bad value',))])
    self.assertRaises(TypeError,
        ShortExpressionFinder, [('x', (1, 'mixed_values'))])
    self.assertRaises(AssertionError,
        ShortExpressionFinder([('no_configs', (1, 2))]).get_expr, [])
    self.assertRaises(AssertionError,
        ShortExpressionFinder([('no_such_value', (1, 2))]).get_expr, [(3,)])
    self.assertRaises(AssertionError,
        ShortExpressionFinder([('wrong_length', (1, 2))]).get_expr, [(1, 3)])

  def test_partitions(self):
    self.assertEqual(((None, None),), tuple(partitions(0, 2)))
    self.assertEqual((), tuple(partitions(1, 2)))

    def count_partitions(parts, remaining):
      parts = tuple(parts)
      if parts == ((None, None),):
        self.assertEqual(0, remaining)
        return 1
      return sum(count_partitions(ns, remaining - n) for n, ns in parts)

    # http://oeis.org/A002865
    expected = [1, 0, 1, 1, 2, 2, 4, 4, 7, 8, 12, 14, 21, 24, 34, 41, 55, 66]
    for n, count in enumerate(expected):
      actual_count = count_partitions(partitions(n, 2), n)
      self.assertEqual(count, actual_count)

    # This also checks that the call doesn't compute all
    # 23,127,843,459,154,899,464,880,444,632,250 partitions up front.
    self.assertEqual(501, len(tuple(partitions(1000, 1))))

  def test_nones(self):
    ShortExpressionFinder([('foo', ('a',))])
    with self.assertRaises(TypeError):
      ShortExpressionFinder([('foo', ('a', None))])

  def test_complex(self):
    # An end-to-end test that looks at behavior with 4 degrees (variables).
    variables = [
        # For example component build vs static build.
        ('lib', ('s', 'c')),
        ('cros', (0, 1)),
        ('OS', ('l', 'm', 'w')),
        ('brand', ('C', 'GC', 'Y')),
    ]
    expectations = [
        (
          [('s', 0, 'l', 'C')],
          'lib=="s" and cros==0 and OS=="l" and brand=="C"',
        ),
        (
          [('s', 0, 'l', 'C'), ('s', 0, 'm', 'C')],
          'lib=="s" and cros==0 and (OS=="l" or OS=="m") and brand=="C"',
        ),
        (
          [('s', 0, 'l', 'C'), ('s', 0, 'm', 'C'), ('s', 0, 'm', 'GC')],
          '(lib=="s" and cros==0 and OS=="l" and brand=="C") or '
          '(lib=="s" and cros==0 and OS=="m" and (brand=="C" or brand=="GC"))'
        ),
        (
          [('s', 0, 'l', 'C'), ('s', 0, 'm', 'C'), ('s', 0, 'w', 'C')],
          'lib=="s" and cros==0 and (OS=="l" or OS=="m" or OS=="w") and '
          'brand=="C"',
        ),
        (
          [
            ('s', 1, 'l', 'C'), ('s', 0, 'l', 'C'), ('s', 0, 'm', 'C'),
            ('s', 0, 'w', 'C'),
          ],
          # TODO(maruel): Could have been written slightly more efficiently.
          # 'lib=="s" and 'brand=="C" and (...)'
          '(lib=="s" and cros==1 and OS=="l" and brand=="C") or '
          '(lib=="s" and cros==0 and (OS=="l" or OS=="m" or OS=="w") and '
          'brand=="C")'
        ),
        (
          [('s', None, None, None)],
          'lib=="s"',
        ),
        (
          [('s', 1, None, None)],
          'lib=="s" and cros==1',
        ),
        (
          [(None, 1, None, None)],
          'cros==1',
        ),
        (
          [(None, 1, None, None), (None, 0, 'w', None), (None, 0, 'm', None)],
          # Note the ordering of the specification is lost.
          '(cros==0 and (OS=="m" or OS=="w")) or cros==1',
        ),
        (
          [('s', 1, 'l', None), ('c', 0, 'w', None), ('s', 0, 'm', None)],
          # TODO(maruel): Could have been written slightly more efficiently.
          '(lib=="s" and cros==0 and OS=="m") or '
          '(lib=="c" and cros==0 and OS=="w") or '
          '(lib=="s" and cros==1 and OS=="l")',
        ),
        (
          [('s', 1, 'l', None), ('c', 0, 'w', None), ('c', 0, 'm', None)],
          '(lib=="s" and cros==1 and OS=="l") or '
          '(lib=="c" and cros==0 and (OS=="m" or OS=="w"))',
        ),
        (
          [
            (None, 1, 'l', 'GC'), (None, 0, 'l', 'GC'), (None, None, 'm', 'GC'),
            (None, None, 'w', 'GC'),
          ],
          '((cros==0 or cros==1) and OS=="l" and brand=="GC") or '
          '((OS=="m" or OS=="w") and brand=="GC")',
        ),
    ]
    for i, (data, expected) in enumerate(expectations):
      s = short_expression_finder.ShortExpressionFinder(variables)
      actual = s.get_expr(data)
      self.assertEqual(
          expected, actual, '\n%r\n%r\n%r\n%s' % (data, expected, actual, i))


class TestPrivateCode(unittest.TestCase):
  # Tests the non-exported functions.
  def test_calculate_cost_simple(self):
    values = ([17],)
    self.assertEqual(
        {2: {1: (1,)}}, short_expression_finder._calculate_costs(values, 1, 1))

  def test_calculate_cost_1_x_0(self):
    # Degenerative case.
    values = ([17], [])
    self.assertEqual({}, short_expression_finder._calculate_costs(values, 1, 1))

  def test_calculate_cost_1_x_2(self):
    values = ([1, 2], [3])
    expected = {
      3: {
        1: (1, 1),
        2: (2, 1),
      },
      5: {
        3: (3, 1),
      },
    }
    self.assertEqual(
        expected, short_expression_finder._calculate_costs(values, 1, 1))

  def test_calculate_cost_2_x_2(self):
    values = ([1, 2], [3, 4])
    expected = {
      3: {
        1: (1, 1),
        2: (2, 1),
        4: (1, 2),
        8: (2, 2),
      },
      5: {
        3: (3, 1),
        5: (1, 3),
        10: (2, 3),
        12: (3, 2),
      },
      7: {
        15: (3, 3),
      },
    }
    self.assertEqual(
        expected, short_expression_finder._calculate_costs(values, 1, 1))

  def test_format_or(self):
    values = ('linux', 'mac', 'win')
    expectations = [
        # No variable is bound. It's the 'global variable'.
        (0, ('', 0)),
        (1, ('OS=="linux"', 1)),
        (2, ('OS=="mac"', 1)),
        (3, ('OS=="linux" or OS=="mac"', 2)),
        (4, ('OS=="win"', 1)),
        (5, ('OS=="linux" or OS=="win"', 2)),
        (6, ('OS=="mac" or OS=="win"', 2)),
        (7, ('OS=="linux" or OS=="mac" or OS=="win"', 3)),
    ]
    for data, expected in expectations:
      self.assertEqual(
          expected, short_expression_finder._format_or('OS', values, data))

  def test_format_and(self):
    # Create a 2D matrix.
    variables = ('chromeos', 'OS')
    values = ((0, 1), ('linux', 'mac', 'win'))
    expectations = [
        # No variable is bound. It's the 'global variable'.
        ((0, 0), ('', 0)),
        ((0, 1), ('OS=="linux"', 1)),
        ((0, 2), ('OS=="mac"', 1)),
        ((0, 3), ('OS=="linux" or OS=="mac"', 1)),
        ((1, 4), ('chromeos==0 and OS=="win"', 2)),
        ((1, 5), ('chromeos==0 and (OS=="linux" or OS=="win")', 2)),
        # _format_and() just renders what it receives.
        (
          (3, 5),
          ('(chromeos==0 or chromeos==1) and (OS=="linux" or OS=="win")', 2),
        ),
    ]
    for data, expected in expectations:
      self.assertEqual(
          expected,
          short_expression_finder._format_and(variables, values, data))

  def test_format_expr(self):
    # Create a 2D matrix.
    variables = ('chromeos', 'OS')
    values = ((0, 1), ('linux', 'mac', 'win'))
    expectations = [
        (
          # Unbounded variable has a 0 as its bitfield.
          ((0, 0),),
          ('', 0),
        ),
        (
          # Unbounded variable has a 0 as its bitfield.
          ((0, 1),),
          ('OS=="linux"', 1),
        ),
        (
          # The function expects already reduced and sorted equations. It won't
          # reduce it on your behalf.
          ((0, 1),(2, 1)),
          ('OS=="linux" or (chromeos==1 and OS=="linux")', 2),
        ),
        (
          ((2, 0),(0, 4)),
          ('chromeos==1 or OS=="win"', 2),
        ),
        (
          # Notice smart use of parenthesis.
          ((1, 1),(0, 5)),
          ('(chromeos==0 and OS=="linux") or OS=="linux" or OS=="win"', 2),
        ),
    ]
    for data, expected in expectations:
      self.assertEqual(
          expected,
          short_expression_finder._format_expr(variables, values, data))

  def test_internal_guts_2_variables(self):
    # chromeos can be: 0 or 1.
    # OS can be: linux, mac or win.
    variables = ('chromeos', 'OS')
    values = ((0, 1), ('linux', 'mac', 'win'))

    # 1. is input data to _get_expr. It is all the possibilities that should be
    #    be matched for.
    # 2. is (bits, values) output from _get_expr, that is input in
    #    _find_reduced_cost.
    # 3. is (assertions) output from _find_reduced_cost, that is input in
    #    _format_expr.
    # 4. is string output from _format_expr.
    expectations = [
        (
          [(0, 'linux')],
          1L,
          ((1, 1),),
          ('chromeos==0 and OS=="linux"', 2),
        ),
        (
          [(1, 'linux'), (0, 'mac')],
          6L,
          ((2, 1), (1, 2)),
          ('(chromeos==1 and OS=="linux") or (chromeos==0 and OS=="mac")', 2),
        ),
        (
          [(1, 'linux'), (0, 'mac'), (0, 'win')],
          22L,
          ((2, 1), (1, 6)),
          (
            '(chromeos==1 and OS=="linux") or '
            '(chromeos==0 and (OS=="mac" or OS=="win"))',
            2,
          ),
        ),
    ]
    for line in expectations:
      # Note that none of these functions support free variable. It's
      # implemented by ShortExpressionFinder.get_expr(.
      to_get_expr, bits, to_format_expr, final_expected = line
      self.assertEqual(
          bits,
          short_expression_finder._get_expr(values, to_get_expr))
      self.assertEqual(
          to_format_expr,
          short_expression_finder._find_reduced_cost(1, 1, values, bits))
      self.assertEqual(
          final_expected,
          short_expression_finder._format_expr(
            variables, values, to_format_expr))

  def test_internal_guts_4_variables(self):
    # Create a 4D matrix.
    # See test_internal_guts_2_variables for the explanations.
    variables = ('lib', 'cros', 'OS', 'brand')
    values = (('s', 'c'), (0, 1), ('l', 'm', 'w'), ('C', 'GC', 'Y'))
    expectations = [
        (
          [('s', 0, 'l', 'C')],
          1L,
          ((1, 1, 1, 1),),
          ('lib=="s" and cros==0 and OS=="l" and brand=="C"', 4),
        ),
        (
          # 2nd value for 1th variable.
          [('c', 0, 'l', 'C')],
          2L,
          ((2, 1, 1, 1),),
          ('lib=="c" and cros==0 and OS=="l" and brand=="C"', 4),
        ),
        (
          # 2nd value for 2th variable.
          [('s', 1, 'l', 'C')],
          4L,
          ((1, 2, 1, 1),),
          ('lib=="s" and cros==1 and OS=="l" and brand=="C"', 4),
        ),
        (
          # 2nd value for 3th variable.
          [('s', 0, 'm', 'C')],
          16L,
          ((1, 1, 2, 1),),
          ('lib=="s" and cros==0 and OS=="m" and brand=="C"', 4),
        ),
        (
          # 3nd value for 3th variable.
          [('s', 0, 'w', 'C')],
          256L,
          ((1, 1, 4, 1),),  # bitfields, not numbers.
          ('lib=="s" and cros==0 and OS=="w" and brand=="C"', 4),
        ),
        (
          # 2nd value for 4th variable.
          [('s', 0, 'l', 'GC')],
          4096L,
          ((1, 1, 1, 2),),
          ('lib=="s" and cros==0 and OS=="l" and brand=="GC"', 4),
        ),
        (
          # Last bit that can be set, all values are the last.
          # 100000000000000000000000000000000000 is 36th bit == 2*2*3*3.
          [('c', 1, 'w', 'Y')],
          34359738368L,
          ((2, 2, 4, 4),),
          ('lib=="c" and cros==1 and OS=="w" and brand=="Y"', 4),
        ),
        (
          # Same condition twice doesn't affect the result.
          [('s', 0, 'l', 'C'), ('s', 0, 'l', 'C')],
          # One real condition, only one bit set.
          1L,
          ((1, 1, 1, 1),),
          ('lib=="s" and cros==0 and OS=="l" and brand=="C"', 4),
        ),
        (
          # All values for 1st variable.
          [('s', 0, 'l', 'C'), ('c', 0, 'l', 'C')],
          # It has 2 bits set.
          3L,
          ((3, 1, 1, 1),),
          ('(lib=="s" or lib=="c") and cros==0 and OS=="l" and brand=="C"', 4),
        ),
        (
          # All values for 2nd variable.
          [('s', 0, 'l', 'C'), ('s', 1, 'l', 'C')],
          # It has 2 bits set.
          5L,
          ((1, 3, 1, 1),),
          ('lib=="s" and (cros==0 or cros==1) and OS=="l" and brand=="C"', 4),
        ),
    ]
    for line in expectations:
      # Note that none of these functions support free variable. It's
      # implemented by ShortExpressionFinder.get_expr().
      to_get_expr, bits, to_format_expr, final_expected = line
      self.assertEqual(
          bits,
          short_expression_finder._get_expr(values, to_get_expr))
      self.assertEqual(
          to_format_expr,
          short_expression_finder._find_reduced_cost(1, 1, values, bits))
      self.assertEqual(
          final_expected,
          short_expression_finder._format_expr(
            variables, values, to_format_expr))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  unittest.main()
