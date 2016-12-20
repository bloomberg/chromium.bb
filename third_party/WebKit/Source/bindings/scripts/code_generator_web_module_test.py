# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import,protected-access

"""Unit tests for code_generator_web_module.py."""

import unittest

from code_generator_web_module import InterfaceContextBuilder
from code_generator_web_module import TypeResolver
from code_generator_web_module import STRING_INCLUDE_PATH
from idl_definitions import IdlAttribute
from idl_definitions import IdlOperation
from idl_types import IdlType
from idl_types import PRIMITIVE_TYPES
from idl_types import STRING_TYPES


# TODO(dglazkov): Convert to use actual objects, not stubs.
# See http://crbug.com/673214 for more details.
class IdlTestingHelper(object):
    """A collection of stub makers and helper utils to make testing code
    generation easy."""

    def make_stub_idl_attribute(self, name, return_type):
        idl_attribute_stub = IdlAttribute()
        idl_attribute_stub.name = name
        idl_attribute_stub.idl_type = IdlType(return_type)
        return idl_attribute_stub

    def make_stub_idl_operation(self, name, return_type):
        idl_operation_stub = IdlOperation()
        idl_operation_stub.name = name
        idl_operation_stub.idl_type = IdlType(return_type)
        return idl_operation_stub

    def make_stub_idl_type(self, base_type):
        return IdlType(base_type)


class TypeResolverTest(unittest.TestCase):

    def test_includes_from_type_should_filter_primitive_types(self):
        type_resolver = TypeResolver()
        helper = IdlTestingHelper()
        for primitive_type in PRIMITIVE_TYPES:
            idl_type = helper.make_stub_idl_type(primitive_type)
            self.assertEqual(
                type_resolver._includes_from_type(idl_type), set())

    def test_includes_from_type_should_filter_void(self):
        type_resolver = TypeResolver()
        helper = IdlTestingHelper()
        idl_type = helper.make_stub_idl_type('void')
        self.assertEqual(
            type_resolver._includes_from_type(idl_type), set())

    def test_includes_from_type_should_handle_string(self):
        type_resolver = TypeResolver()
        helper = IdlTestingHelper()
        for string_type in STRING_TYPES:
            idl_type = helper.make_stub_idl_type(string_type)
            self.assertEqual(
                type_resolver._includes_from_type(idl_type),
                set([STRING_INCLUDE_PATH]))


class InterfaceContextBuilderTest(unittest.TestCase):

    def test_empty(self):
        builder = InterfaceContextBuilder('test', TypeResolver())

        self.assertEqual({'code_generator': 'test'}, builder.build())

    def test_set_name(self):
        builder = InterfaceContextBuilder('test', TypeResolver())

        builder.set_class_name('foo')
        self.assertEqual({
            'code_generator': 'test',
            'class_name': 'foo',
        }, builder.build())

    def test_set_inheritance(self):
        builder = InterfaceContextBuilder('test', TypeResolver())
        builder.set_inheritance('foo')
        self.assertEqual({
            'code_generator': 'test',
            'inherits_expression': ' : public foo',
            'cpp_includes': set(['foo']),
        }, builder.build())

        builder = InterfaceContextBuilder('test', TypeResolver())
        builder.set_inheritance(None)
        self.assertEqual({'code_generator': 'test'}, builder.build())


    def test_add_attribute(self):
        helper = IdlTestingHelper()
        builder = InterfaceContextBuilder('test', TypeResolver())

        attribute = helper.make_stub_idl_attribute('foo', 'bar')
        builder.add_attribute(attribute)
        self.assertEqual({
            'code_generator': 'test',
            'cpp_includes': set(['bar']),
            'attributes': [{'name': 'foo', 'return_type': 'bar'}],
        }, builder.build())

    def test_add_method(self):
        helper = IdlTestingHelper()
        builder = InterfaceContextBuilder('test', TypeResolver())

        operation = helper.make_stub_idl_operation('foo', 'bar')
        builder.add_operation(operation)
        self.assertEqual({
            'code_generator': 'test',
            'cpp_includes': set(['bar']),
            'methods': [{'name': 'foo', 'return_type': 'bar'}],
        }, builder.build())
