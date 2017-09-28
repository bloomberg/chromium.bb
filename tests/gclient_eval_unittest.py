#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import itertools
import logging
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from third_party import schema

import gclient
import gclient_eval


class GClientEvalTest(unittest.TestCase):
  def test_str(self):
    self.assertEqual('foo', gclient_eval._gclient_eval('"foo"', {}))

  def test_tuple(self):
    self.assertEqual(('a', 'b'), gclient_eval._gclient_eval('("a", "b")', {}))

  def test_list(self):
    self.assertEqual(['a', 'b'], gclient_eval._gclient_eval('["a", "b"]', {}))

  def test_dict(self):
    self.assertEqual({'a': 'b'}, gclient_eval._gclient_eval('{"a": "b"}', {}))

  def test_name_safe(self):
    self.assertEqual(True, gclient_eval._gclient_eval('True', {}))

  def test_name_unsafe(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_eval('UnsafeName', {'UnsafeName': 'foo'})
    self.assertIn('invalid name \'UnsafeName\'', str(cm.exception))

  def test_call(self):
    self.assertEqual(
        'bar',
        gclient_eval._gclient_eval('Foo("bar")', {'Foo': lambda x: x}))

  def test_plus(self):
    self.assertEqual('foo', gclient_eval._gclient_eval('"f" + "o" + "o"', {}))

  def test_format(self):
    self.assertEqual('foo', gclient_eval._gclient_eval('"%s" % "foo"', {}))

  def test_not_expression(self):
    with self.assertRaises(SyntaxError) as cm:
      gclient_eval._gclient_eval('def foo():\n  pass', {})
    self.assertIn('invalid syntax', str(cm.exception))

  def test_not_whitelisted(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_eval('[x for x in [1, 2, 3]]', {})
    self.assertIn(
        'unexpected AST node: <_ast.ListComp object', str(cm.exception))

  def test_dict_ordered(self):
    for test_case in itertools.permutations(range(4)):
      input_data = ['{'] + ['"%s": "%s",' % (n, n) for n in test_case] + ['}']
      expected = [(str(n), str(n)) for n in test_case]
      result = gclient_eval._gclient_eval(''.join(input_data), {})
      self.assertEqual(expected, result.items())


class ExecTest(unittest.TestCase):
  def test_multiple_assignment(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval.Exec('a, b, c = "a", "b", "c"', {}, {})
    self.assertIn(
        'invalid assignment: target should be a name', str(cm.exception))

  def test_override(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval.Exec('a = "a"\na = "x"', {}, {})
    self.assertIn(
        'invalid assignment: overrides var \'a\'', str(cm.exception))

  def test_schema_unknown_key(self):
    with self.assertRaises(schema.SchemaWrongKeyError):
      gclient_eval.Exec('foo = "bar"', {}, {}, '<string>')

  def test_schema_wrong_type(self):
    with self.assertRaises(schema.SchemaError):
      gclient_eval.Exec('include_rules = {}', {}, {}, '<string>')

  def test_recursedeps_list(self):
    local_scope = {}
    gclient_eval.Exec(
        'recursedeps = [["src/third_party/angle", "DEPS.chromium"]]',
        {}, local_scope,
        '<string>')
    self.assertEqual(
        {'recursedeps': [['src/third_party/angle', 'DEPS.chromium']]},
        local_scope)

  def test_var(self):
    local_scope = {}
    global_scope = {
        'Var': lambda var_name: '{%s}' % var_name,
    }
    gclient_eval.Exec('\n'.join([
        'vars = {',
        '  "foo": "bar",',
        '}',
        'deps = {',
        '  "a_dep": "a" + Var("foo") + "b",',
        '}',
    ]), global_scope, local_scope, '<string>')
    self.assertEqual({
        'vars': collections.OrderedDict([('foo', 'bar')]),
        'deps': collections.OrderedDict([('a_dep', 'a{foo}b')]),
    }, local_scope)


class EvaluateConditionTest(unittest.TestCase):
  def test_true(self):
    self.assertTrue(gclient_eval.EvaluateCondition('True', {}))

  def test_variable(self):
    self.assertFalse(gclient_eval.EvaluateCondition('foo', {'foo': 'False'}))

  def test_variable_cyclic_reference(self):
    with self.assertRaises(ValueError) as cm:
      self.assertTrue(gclient_eval.EvaluateCondition('bar', {'bar': 'bar'}))
    self.assertIn(
        'invalid cyclic reference to \'bar\' (inside \'bar\')',
        str(cm.exception))

  def test_operators(self):
    self.assertFalse(gclient_eval.EvaluateCondition(
        'a and not (b or c)', {'a': 'True', 'b': 'False', 'c': 'True'}))

  def test_expansion(self):
    self.assertTrue(gclient_eval.EvaluateCondition(
        'a or b', {'a': 'b and c', 'b': 'not c', 'c': 'False'}))

  def test_string_equality(self):
    self.assertFalse(gclient_eval.EvaluateCondition(
        'foo == "bar"', {'foo': '"baz"'}))

  def test_string_bool(self):
    self.assertFalse(gclient_eval.EvaluateCondition(
        'false_str_var and true_var',
        {'false_str_var': 'False', 'true_var': True}))

  def test_string_bool_typo(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval.EvaluateCondition(
          'false_var_str and true_var',
          {'false_str_var': 'False', 'true_var': True})
    self.assertIn(
        'invalid "and" operand \'false_var_str\' '
            '(inside \'false_var_str and true_var\')',
        str(cm.exception))


if __name__ == '__main__':
  level = logging.DEBUG if '-v' in sys.argv else logging.FATAL
  logging.basicConfig(
      level=level,
      format='%(asctime).19s %(levelname)s %(filename)s:'
             '%(lineno)s %(message)s')
  unittest.main()
