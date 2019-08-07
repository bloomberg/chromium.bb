# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import exceptions
from .common import WithCodeGeneratorInfo
from .common import WithComponent
from .common import WithDebugInfo
from .common import WithExtendedAttributes
from .common import WithIdentifier

# This file defines following classes to provide IDL types
#
# Classes:
# -IdlType
# -  NumberType
# -    IntegerType
# -    RealNumberType
# -  BooleanType
# -  StringType
# -  AnyType
# -  ObjectType
# -  VoidType
# -  ArrayLikeType
# -    SequenceType
# -    FrozenArrayType
# -  RecordType
# -  PromiseType
# -  UnionType
# -  NullableType
# -  AnnotatedType


class IdlType(WithCodeGeneratorInfo):
    @property
    def type_name(self):
        """
        Returns type name as defined in WebIDL spec; e.g. LongLongOrNullClamp
        for '[Clamp] long long?'.
        @return Identifier
        """
        # TODO(peria): Replace with exceptions.NotImplementedError() after shipping.
        assert 'type_name() is not implemented for class %s' % (type(self))

    # These flags are used for types in args.
    @property
    def is_optional(self):
        """
        Returns True if this type is used in an optional argument.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_variadic(self):
        """
        Returns True if this type is used in a variadic argument.
        @return bool
        """
        raise exceptions.NotImplementedError()

    # Some of following type checkers return True, if |this| meets the
    # expectation.
    @property
    def is_number_type(self):
        """
        Returns True if |self| is a NumberType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_integer_type(self):
        """
        Returns True if |self| is an IntegerType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_real_number_type(self):
        """
        Returns True if |self| is a RealNumberType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_boolean_type(self):
        """
        Returns True if |self| is a BooleanType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_string_type(self):
        """
        Returns True if |self| is a StringType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_object_type(self):
        """
        Returns True if |self| is an ObjectType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_any_type(self):
        """
        Returns True if |self| is an AnyType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_interface_type(self):
        """
        Returns True if |self| is an InterfaceType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_dictionary_type(self):
        """
        Returns True if |self| is a DictionaryType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_namespace_type(self):
        """
        Returns True if |self| is a NamespaceType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_enumeration_type(self):
        """
        Returns True if |self| is an EnumerationType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_callback_interface_type(self):
        """
        Returns True if |self| is a CallbackInterfaceType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_callback_function_type(self):
        """
        Returns True if |self| is a CallbackFunctionType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_void_type(self):
        """
        Returns True if |self| is a VoidType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_nullable_type(self):
        """
        Returns True if |self| is a NullableType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_annotated_type(self):
        """
        Returns True if |self| is an AnnotatedType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_promise_type(self):
        """
        Returns True if |self| is a PromiseType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_record_type(self):
        """
        Returns True if |self| is a RecordType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_sequence_type(self):
        """
        Returns True if |self| is a SequenceType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def is_union_type(self):
        """
        Returns True if |self| is a UnionType.
        @return bool
        """
        raise exceptions.NotImplementedError()

    @property
    def typedefs(self):
        """
        Returns Typedef instances which directly point this type.
        @return tuple(Typedef)
        """
        raise exceptions.NotImplementedError()

    @property
    def inner_type(self):
        """
        Returns inner type if |this| is a nullable or an annotated type.
        @return IdlType?
        """
        raise exceptions.NotImplementedError()

    # One of following methods returns an instance, if |this| refers a definition.
    @property
    def interface(self):
        """
        Returns the interface that |self| points if |self| is an InterfaceType.
        @return Interface
        """
        raise exceptions.NotImplementedError()

    @property
    def dictionary(self):
        """
        Returns the dictionary that |self| points if |self| is an DictionaryType.
        @return Dictionary
        """
        raise exceptions.NotImplementedError()

    @property
    def namespace(self):
        """
        Returns the namespace that |self| points if |self| is an NamespaceType.
        @return Namespace
        """
        raise exceptions.NotImplementedError()

    @property
    def enumeration(self):
        """
        Returns the enumeration that |self| points if |self| is an EnumerationType.
        @return Enumeration
        """
        raise exceptions.NotImplementedError()

    @property
    def callback_interface(self):
        """
        Returns the callback interface that |self| points if |self| is an CallbackInterfaceType.
        @return CallbackInterface
        """
        raise exceptions.NotImplementedError()

    @property
    def callback_function(self):
        """
        Returns the callback function that |self| points if |self| is an CallbackFunctionType.
        @return CallbackFunction
        """
        raise exceptions.NotImplementedError()


class NumberType(IdlType, WithIdentifier):
    """https://heycam.github.io/webidl/#idl-types"""

    # IdlType overrides
    @property
    def is_number_type(self):
        raise exceptions.NotImplementedError()


class IntegerType(IdlType, WithIdentifier):
    """https://heycam.github.io/webidl/#idl-types"""

    # IdlType overrides
    @property
    def is_integer_type(self):
        raise exceptions.NotImplementedError()


class RealNumberType(IdlType, WithIdentifier):
    """https://heycam.github.io/webidl/#idl-types"""

    @property
    def is_unrestricted(self):
        """
        Returns True if 'unrestricted' is specified.
        @return bool
        """
        raise exceptions.NotImplementedError()

    # IdlType overrides
    @property
    def is_real_number_type(self):
        raise exceptions.NotImplementedError()


class BooleanType(IdlType, WithIdentifier):
    """https://heycam.github.io/webidl/#idl-types"""

    # IdlType overrides
    @property
    def is_boolean_type(self):
        raise exceptions.NotImplementedError()


class StringType(IdlType, WithIdentifier):
    """https://heycam.github.io/webidl/#idl-types"""

    # IdlType overrides
    @property
    def is_string_type(self):
        raise exceptions.NotImplementedError()


class AnyType(IdlType):

    # IdlType overrides
    @property
    def is_any_type(self):
        raise exceptions.NotImplementedError()


class ObjectType(IdlType):
    # IdlType overrides
    @property
    def is_object_type(self):
        raise exceptions.NotImplementedError()


class VoidType(IdlType):
    # IdlType overrides
    @property
    def is_void_type(self):
        raise exceptions.NotImplementedError()


class ArrayLikeType(IdlType):
    @property
    def element_type(self):
        raise exceptions.NotImplementedError()


class SequenceType(ArrayLikeType):
    # IdlType overrides
    @property
    def is_sequence_type(self):
        raise exceptions.NotImplementedError()


class FrozenArrayType(ArrayLikeType):
    # IdlType overrides
    @property
    def is_frozen_array_type(self):
        raise exceptions.NotImplementedError()


class RecordType(IdlType):
    @property
    def key_type(self):
        """
        Returns the key type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    @property
    def value_type(self):
        """
        Returns the value type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    # IdlType overrides
    @property
    def is_record_type(self):
        raise exceptions.NotImplementedError()


class PromiseType(IdlType):
    @property
    def result_type(self):
        """
        Returns the result type.
        @return IdlType
        """
        raise exceptions.NotImplementedError()

    # IdlType overrides
    @property
    def is_promise_type(self):
        raise exceptions.NotImplementedError()


class UnionType(IdlType, WithComponent, WithDebugInfo):
    @property
    def member_types(self):
        """
        Returns a list of member types.
        @return tuple(IdlType)
        """
        raise exceptions.NotImplementedError()

    @property
    def flattened_member_types(self):
        """
        Returns a set of flattened member types.
        https://heycam.github.io/webidl/#dfn-flattened-union-member-types
        @return set(IdlType)
        """
        raise exceptions.NotImplementedError()

    # IdlType overrides
    @property
    def is_union_type(self):
        raise exceptions.NotImplementedError()


class NullableType(IdlType):
    # IdlType overrides
    @property
    def is_nullable_type(self):
        raise exceptions.NotImplementedError()

    @property
    def inner_type(self):
        raise exceptions.NotImplementedError()


class AnnotatedType(IdlType, WithExtendedAttributes):
    # IdlType overrides
    @property
    def is_annotated_type(self):
        raise exceptions.NotImplementedError()

    @property
    def inner_type(self):
        raise exceptions.NotImplementedError()
