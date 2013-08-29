#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils.short_expression_finder import ShortExpressionFinder, partitions


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


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  unittest.main()
