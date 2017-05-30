#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from third_party import schema

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


class GClientExecTest(unittest.TestCase):
  def test_basic(self):
    self.assertEqual(
        {'a': '1', 'b': '2', 'c': '3'},
        gclient_eval._gclient_exec('a = "1"\nb = "2"\nc = "3"', {}))

  def test_multiple_assignment(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_exec('a, b, c = "a", "b", "c"', {})
    self.assertIn(
        'invalid assignment: target should be a name', str(cm.exception))

  def test_override(self):
    with self.assertRaises(ValueError) as cm:
      gclient_eval._gclient_exec('a = "a"\na = "x"', {})
    self.assertIn(
        'invalid assignment: overrides var \'a\'', str(cm.exception))


class CheckTest(unittest.TestCase):
  TEST_CODE="""
include_rules = ["a", "b", "c"]

vars = {"a": "1", "b": "2", "c": "3"}

deps_os = {
  "linux": {"a": "1", "b": "2", "c": "3"}
}"""

  def setUp(self):
    self.expected = {}
    exec(self.TEST_CODE, {}, self.expected)

  def test_pass(self):
    gclient_eval.Check(self.TEST_CODE, '<string>', {}, self.expected)

  def test_fail_list(self):
    self.expected['include_rules'][0] = 'x'
    with self.assertRaises(gclient_eval.CheckFailure):
      gclient_eval.Check(self.TEST_CODE, '<string>', {}, self.expected)

  def test_fail_dict(self):
    self.expected['vars']['a'] = 'x'
    with self.assertRaises(gclient_eval.CheckFailure):
      gclient_eval.Check(self.TEST_CODE, '<string>', {}, self.expected)

  def test_fail_nested(self):
    self.expected['deps_os']['linux']['c'] = 'x'
    with self.assertRaises(gclient_eval.CheckFailure):
      gclient_eval.Check(self.TEST_CODE, '<string>', {}, self.expected)

  def test_schema_unknown_key(self):
    with self.assertRaises(schema.SchemaWrongKeyError):
      gclient_eval.Check('foo = "bar"', '<string>', {}, {'foo': 'bar'})

  def test_schema_wrong_type(self):
    with self.assertRaises(schema.SchemaError):
      gclient_eval.Check(
          'include_rules = {}', '<string>', {}, {'include_rules': {}})

  def test_recursedeps_list(self):
    gclient_eval.Check(
        'recursedeps = [["src/third_party/angle", "DEPS.chromium"]]',
        '<string>',
        {},
        {'recursedeps': [['src/third_party/angle', 'DEPS.chromium']]})


if __name__ == '__main__':
  level = logging.DEBUG if '-v' in sys.argv else logging.FATAL
  logging.basicConfig(
      level=level,
      format='%(asctime).19s %(levelname)s %(filename)s:'
             '%(lineno)s %(message)s')
  unittest.main()
