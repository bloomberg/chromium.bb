#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import itertools
import logging
import os
import sys
import textwrap
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from third_party import schema

import gclient
import gclient_eval


_SAMPLE_DEPS_FILE = textwrap.dedent("""\
vars = {
    'git_repo': 'https://example.com/repo.git',
     # Some comment with bad indentation
     'dep_2_rev': '1ced',
     # Some more comments
     # 1 
     # 2
     # 3
    'dep_3_rev': '5p1e5',
}

deps = {
    'src/dep': Var('git_repo') + '/dep' + '@' + 'deadbeef',
    # Some comment
    'src/android/dep_2': {
        'url': Var('git_repo') + '/dep_2' + '@' + Var('dep_2_rev'),
        'condition': 'checkout_android',
    },

    'src/dep_3': Var('git_repo') + '/dep_3@' + Var('dep_3_rev'),

    'src/cipd/package': {
        'packages': [
            {
                'package': 'some/cipd/package',
                'version': 'version:1234',
            },
            {
                'package': 'another/cipd/package',
                'version': 'version:5678',
            },
        ],
        'condition': 'checkout_android',
        'dep_type': 'cipd',
    },
}
""")


class GClientEvalTest(unittest.TestCase):
  def test_str(self):
    self.assertEqual(
        'foo',
        gclient_eval._gclient_eval('"foo"', vars_dict=None, expand_vars=False,
                                   filename='<unknown>'))

  def test_tuple(self):
    self.assertEqual(
        ('a', 'b'),
        gclient_eval._gclient_eval('("a", "b")', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_list(self):
    self.assertEqual(
        ['a', 'b'],
        gclient_eval._gclient_eval('["a", "b"]', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_dict(self):
    self.assertEqual(
        {'a': 'b'},
        gclient_eval._gclient_eval('{"a": "b"}', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_name_safe(self):
    self.assertEqual(
        True,
        gclient_eval._gclient_eval('True', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_name_unsafe(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_eval('UnsafeName', vars_dict=None,
                                 expand_vars=False, filename='<unknown>')
    self.assertIn('invalid name \'UnsafeName\'', str(cm.exception))

  def test_invalid_call(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_eval('Foo("bar")', vars_dict=None,
                                 expand_vars=False, filename='<unknown>')
    self.assertIn('Var is the only allowed function', str(cm.exception))

  def test_call(self):
    self.assertEqual(
        '{bar}',
        gclient_eval._gclient_eval('Var("bar")', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_plus(self):
    self.assertEqual(
        'foo',
        gclient_eval._gclient_eval('"f" + "o" + "o"', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_format(self):
    self.assertEqual(
        'foo',
        gclient_eval._gclient_eval('"%s" % "foo"', vars_dict=None,
                                   expand_vars=False, filename='<unknown>'))

  def test_not_expression(self):
    with self.assertRaises(SyntaxError) as cm:
      gclient_eval._gclient_eval(
          'def foo():\n  pass', vars_dict=None, expand_vars=False,
          filename='<unknown>')
    self.assertIn('invalid syntax', str(cm.exception))

  def test_not_whitelisted(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_eval(
          '[x for x in [1, 2, 3]]', vars_dict=None, expand_vars=False,
          filename='<unknown>')
    self.assertIn(
        'unexpected AST node: <_ast.ListComp object', str(cm.exception))

  def test_dict_ordered(self):
    for test_case in itertools.permutations(range(4)):
      input_data = ['{'] + ['"%s": "%s",' % (n, n) for n in test_case] + ['}']
      expected = [(str(n), str(n)) for n in test_case]
      result = gclient_eval._gclient_eval(
          ''.join(input_data), vars_dict=None, expand_vars=False,
          filename='<unknown>')
      self.assertEqual(expected, result.items())


class ExecTest(unittest.TestCase):
  def test_multiple_assignment(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval.Exec('a, b, c = "a", "b", "c"')
    self.assertIn(
        'invalid assignment: target should be a name', str(cm.exception))

  def test_override(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval.Exec('a = "a"\na = "x"')
    self.assertIn(
        'invalid assignment: overrides var \'a\'', str(cm.exception))

  def test_schema_wrong_type(self):
    with self.assertRaises(schema.SchemaError):
      gclient_eval.Exec('include_rules = {}')

  def test_recursedeps_list(self):
    local_scope = gclient_eval.Exec(
        'recursedeps = [["src/third_party/angle", "DEPS.chromium"]]')
    self.assertEqual(
        {'recursedeps': [['src/third_party/angle', 'DEPS.chromium']]},
        local_scope)

  def test_var(self):
    local_scope = gclient_eval.Exec('\n'.join([
        'vars = {',
        '  "foo": "bar",',
        '}',
        'deps = {',
        '  "a_dep": "a" + Var("foo") + "b",',
        '}',
    ]))
    self.assertEqual({
        'vars': collections.OrderedDict([('foo', 'bar')]),
        'deps': collections.OrderedDict([('a_dep', 'abarb')]),
    }, local_scope)

  def test_braces_var(self):
    local_scope = gclient_eval.Exec('\n'.join([
        'vars = {',
        '  "foo": "bar",',
        '}',
        'deps = {',
        '  "a_dep": "a{foo}b",',
        '}',
    ]))
    self.assertEqual({
        'vars': collections.OrderedDict([('foo', 'bar')]),
        'deps': collections.OrderedDict([('a_dep', 'abarb')]),
    }, local_scope)

  def test_var_unexpanded(self):
    local_scope = gclient_eval.Exec('\n'.join([
        'vars = {',
        '  "foo": "bar",',
        '}',
        'deps = {',
        '  "a_dep": "a" + Var("foo") + "b",',
        '}',
    ]), expand_vars=False)
    self.assertEqual({
        'vars': collections.OrderedDict([('foo', 'bar')]),
        'deps': collections.OrderedDict([('a_dep', 'a{foo}b')]),
    }, local_scope)

  def test_empty_deps(self):
    local_scope = gclient_eval.Exec('deps = {}', '<string>')
    self.assertEqual({'deps': {}}, local_scope)


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
    self.assertTrue(gclient_eval.EvaluateCondition(
        'foo == "baz"', {'foo': '"baz"'}))
    self.assertFalse(gclient_eval.EvaluateCondition(
        'foo == "bar"', {'foo': '"baz"'}))

  def test_string_inequality(self):
    self.assertTrue(gclient_eval.EvaluateCondition(
        'foo != "bar"', {'foo': '"baz"'}))
    self.assertFalse(gclient_eval.EvaluateCondition(
        'foo != "baz"', {'foo': '"baz"'}))

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


class SetVarTest(unittest.TestCase):
  def testSetVar(self):
    local_scope = gclient_eval.Exec(_SAMPLE_DEPS_FILE)

    gclient_eval.SetVar(local_scope, 'dep_2_rev', 'c0ffee')
    result = gclient_eval.RenderDEPSFile(local_scope)

    self.assertEqual(
        result,
        _SAMPLE_DEPS_FILE.replace('1ced', 'c0ffee'))


class SetCipdTest(unittest.TestCase):
  def testSetCIPD(self):
    local_scope = gclient_eval.Exec(_SAMPLE_DEPS_FILE)

    gclient_eval.SetCIPD(
        local_scope, 'src/cipd/package', 'another/cipd/package', '6.789')
    result = gclient_eval.RenderDEPSFile(local_scope)

    self.assertEqual(result, _SAMPLE_DEPS_FILE.replace('5678', '6.789'))


class SetRevisionTest(unittest.TestCase):
  def setUp(self):
    self.local_scope = gclient_eval.Exec(_SAMPLE_DEPS_FILE)

  def testSetRevision(self):
    gclient_eval.SetRevision(
        self.local_scope, 'src/dep', 'deadfeed')
    result = gclient_eval.RenderDEPSFile(self.local_scope)

    self.assertEqual(result, _SAMPLE_DEPS_FILE.replace('deadbeef', 'deadfeed'))

  def testSetRevisionInUrl(self):
    gclient_eval.SetRevision(
        self.local_scope, 'src/dep_3', '0ff1ce')
    result = gclient_eval.RenderDEPSFile(self.local_scope)

    self.assertEqual(result, _SAMPLE_DEPS_FILE.replace('5p1e5', '0ff1ce'))

  def testSetRevisionInVars(self):
    gclient_eval.SetRevision(
        self.local_scope, 'src/android/dep_2', 'c0ffee')
    result = gclient_eval.RenderDEPSFile(self.local_scope)

    self.assertEqual(result, _SAMPLE_DEPS_FILE.replace('1ced', 'c0ffee'))


if __name__ == '__main__':
  level = logging.DEBUG if '-v' in sys.argv else logging.FATAL
  logging.basicConfig(
      level=level,
      format='%(asctime).19s %(levelname)s %(filename)s:'
             '%(lineno)s %(message)s')
  unittest.main()
