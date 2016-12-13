# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=import-error,print-statement,relative-import

"""Unit tests for code_generator_web_module.py."""

import unittest

from code_generator_web_module import InterfaceContextBuilder


class IdlTestingHelper(object):
    """A collection of stub makers and helper utils to make testing code
    generation easy."""

    def make_stub_object(self):
        return type('', (), {})()

    def add_idl_type_to_stub(self, stub, return_type):
        stub.idl_type = self.make_stub_object()
        stub.idl_type.preprocessed_type = self.make_stub_object()
        stub.idl_type.preprocessed_type.base_type = return_type

    def make_stub_idl_attribute(self, name, return_type):
        idl_attribute_stub = self.make_stub_object()
        idl_attribute_stub.name = name
        self.add_idl_type_to_stub(idl_attribute_stub, return_type)
        return idl_attribute_stub

    def make_stub_idl_operation(self, name, return_type):
        idl_operation_stub = self.make_stub_object()
        idl_operation_stub.name = name
        self.add_idl_type_to_stub(idl_operation_stub, return_type)
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
