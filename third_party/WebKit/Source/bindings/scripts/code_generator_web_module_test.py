# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Unit tests for code_generator_web_module.py."""

import unittest

from code_generator_web_module import InterfaceContextBuilder
from idl_definitions import IdlAttribute
from idl_definitions import IdlOperation
from idl_types import IdlType


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


class InterfaceContextBuilderTest(unittest.TestCase):

    def test_empty(self):
        builder = InterfaceContextBuilder('test')

        self.assertEqual({'code_generator': 'test'}, builder.build())

    def test_set_name(self):
        builder = InterfaceContextBuilder('test')

        builder.set_class_name('foo')
        self.assertEqual({
            'code_generator': 'test',
            'class_name': 'foo',
        }, builder.build())

    def test_set_inheritance(self):
        builder = InterfaceContextBuilder('test')
        builder.set_inheritance('foo')
        self.assertEqual({
            'code_generator': 'test',
            'inherits_expression': ' : public foo',
            'cpp_includes': set(['foo']),
        }, builder.build())

        builder = InterfaceContextBuilder('test')
        builder.set_inheritance(None)
        self.assertEqual({'code_generator': 'test'}, builder.build())


    def test_add_attribute(self):
        helper = IdlTestingHelper()
        builder = InterfaceContextBuilder('test')

        attribute = helper.make_stub_idl_attribute('foo', 'bar')
        builder.add_attribute(attribute)
        self.assertEqual({
            'code_generator': 'test',
            'cpp_includes': set(['bar']),
            'attributes': [{'name': 'foo', 'return_type': 'bar'}],
        }, builder.build())

    def test_add_method(self):
        helper = IdlTestingHelper()
        builder = InterfaceContextBuilder('test')

        operation = helper.make_stub_idl_operation('foo', 'bar')
        builder.add_operation(operation)
        self.assertEqual({
            'code_generator': 'test',
            'cpp_includes': set(['bar']),
            'methods': [{'name': 'foo', 'return_type': 'bar'}],
        }, builder.build())
