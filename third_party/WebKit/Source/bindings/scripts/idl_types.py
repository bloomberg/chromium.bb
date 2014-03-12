# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""IDL type handling.

Classes:
IdlType
IdlUnionType
TypedObject :: mixin for typedef resolution
"""

# pylint doesn't understand ABCs.
# pylint: disable=W0232, E0203, W0201

import abc
from collections import defaultdict

################################################################################
# IDL types
################################################################################

BASIC_TYPES = set([
    # Built-in, non-composite, non-object data types
    # http://www.w3.org/TR/WebIDL/#dfn-primitive-type
    'boolean',
    'float',
    # unrestricted float is not supported
    'double',
    # unrestricted double is not supported
    # http://www.w3.org/TR/WebIDL/#idl-types
    'DOMString',
    'Date',
    # http://www.w3.org/TR/WebIDL/#es-type-mapping
    'void',
])
INTEGER_TYPES = set([
    # http://www.w3.org/TR/WebIDL/#dfn-integer-type
    'byte',
    'octet',
    'short',
    'unsigned short',
    # int and unsigned are not IDL types
    'long',
    'unsigned long',
    'long long',
    'unsigned long long',
])
BASIC_TYPES.update(INTEGER_TYPES)

ancestors = defaultdict(list)  # interface_name -> ancestors
callback_functions = set()
callback_interfaces = set()
enums = {}  # name -> values

# FIXME: move these functions into methods of IdlType
def array_or_sequence_type(idl_type):
    return array_type(idl_type) or sequence_type(idl_type)


def array_type(idl_type):
    if is_union_type(idl_type):
        # We do not support arrays of union types
        return False
    return idl_type.is_array and IdlType(idl_type.base_type)


def is_basic_type(idl_type):
    return str(idl_type) in BASIC_TYPES


def is_integer_type(idl_type):
    return str(idl_type) in INTEGER_TYPES


def is_callback_function(idl_type):
    return str(idl_type) in callback_functions


def set_callback_functions(new_callback_functions):
    callback_functions.update(new_callback_functions)


def is_callback_interface(idl_type):
    return str(idl_type) in callback_interfaces


def set_callback_interfaces(new_callback_interfaces):
    callback_interfaces.update(new_callback_interfaces)


def is_composite_type(idl_type):
    return (str(idl_type) == 'any' or
            array_type(idl_type) or
            sequence_type(idl_type) or
            is_union_type(idl_type))


def is_enum(idl_type):
    return str(idl_type) in enums


def enum_values(idl_type):
    return enums[str(idl_type)]


def set_enums(new_enums):
    enums.update(new_enums)


def inherits_interface(interface_name, ancestor_name):
    return (interface_name == ancestor_name or
            ancestor_name in ancestors[interface_name])


def set_ancestors(new_ancestors):
    ancestors.update(new_ancestors)


def is_interface_type(idl_type):
    # Anything that is not another type is an interface type.
    # http://www.w3.org/TR/WebIDL/#idl-types
    # http://www.w3.org/TR/WebIDL/#idl-interface
    # In C++ these are RefPtr or PassRefPtr types.
    return not(is_basic_type(idl_type) or
               is_composite_type(idl_type) or
               is_callback_function(idl_type) or
               is_enum(idl_type) or
               str(idl_type) == 'object' or
               str(idl_type) == 'Promise')  # Promise will be basic in future


def sequence_type(idl_type):
    if is_union_type(idl_type):
        # We do not support sequences of union types
        return False
    return idl_type.is_sequence and IdlType(idl_type.base_type)


def is_union_type(idl_type):
    return isinstance(idl_type, IdlUnionType)


################################################################################
# Type classes (for typedef resolution)
################################################################################

class TypedObject(object):
    """Object with a type, such as an Attribute or Operation (return value).

    The type can be an actual type, or can be a typedef, which must be resolved
    before passing data to the code generator.
    """
    __metaclass__ = abc.ABCMeta
    idl_type = None

    def resolve_typedefs(self, typedefs):
        """Resolve typedefs to actual types in the object."""
        # Constructors don't have their own return type, because it's the
        # interface itself.
        if not self.idl_type:
            return
        self.idl_type.resolve_typedefs(typedefs)


class IdlType(object):
    # FIXME: incorporate Nullable, etc.
    # FIXME: use nested types: IdlArrayType, IdlNullableType, IdlSequenceType
    # to support types like short?[] vs. short[]?, instead of treating these
    # as orthogonal properties (via flags).
    def __init__(self, base_type, is_array=False, is_sequence=False):
        if is_array and is_sequence:
            raise ValueError('Array of Sequences are not allowed.')
        self.base_type = base_type
        self.is_array = is_array
        self.is_sequence = is_sequence

    def __str__(self):
        type_string = self.base_type
        if self.is_array:
            return type_string + '[]'
        if self.is_sequence:
            return 'sequence<%s>' % type_string
        return type_string

    def resolve_typedefs(self, typedefs):
        if self.base_type in typedefs:
            # FIXME: a bit ugly; use __init__ instead of setting flags
            new_type = typedefs[self.base_type]
            self.base_type = new_type.base_type
            # handles TYPEDEF[] and 'typedef Type[] TYPEDEF'
            self.is_array |= new_type.is_array
            self.is_sequence |= new_type.is_sequence


class IdlUnionType(object):
    # FIXME: derive from IdlType, instead of stand-alone class
    def __init__(self, union_member_types):
        self.union_member_types = union_member_types

    def resolve_typedefs(self, typedefs):
        self.union_member_types = [
            typedefs.get(union_member_type, union_member_type)
            for union_member_type in self.union_member_types]
