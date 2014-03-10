# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""IDL type handling."""

# pylint doesn't understand ABCs.
# pylint: disable=W0232, E0203, W0201

import abc
import re

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

ancestors = {}  # interface_name -> ancestors
callback_functions = set()
callback_interfaces = set()
enums = {}  # name -> values


def array_or_sequence_type(idl_type):
    return array_type(idl_type) or sequence_type(idl_type)


def array_type(idl_type):
    if is_union_type(idl_type):
        # We do not support arrays of union types
        return False
    matched = re.match(r'([\w\s]+)\[\]', idl_type)
    return matched and matched.group(1)


def is_basic_type(idl_type):
    return idl_type in BASIC_TYPES


def is_integer_type(idl_type):
    return idl_type in INTEGER_TYPES


def is_callback_function(idl_type):
    return idl_type in callback_functions


def set_callback_functions(new_callback_functions):
    callback_functions.update(new_callback_functions)


def is_callback_interface(idl_type):
    return idl_type in callback_interfaces


def set_callback_interfaces(new_callback_interfaces):
    callback_interfaces.update(new_callback_interfaces)


def is_composite_type(idl_type):
    return (idl_type == 'any' or
            array_type(idl_type) or
            sequence_type(idl_type) or
            is_union_type(idl_type))


def is_enum(idl_type):
    return idl_type in enums


def enum_values(idl_type):
    return enums.get(idl_type)


def set_enums(new_enums):
    enums.update(new_enums)


def inherits_interface(interface_name, ancestor_name):
    return (interface_name == ancestor_name or
            ancestor_name in ancestors.get(interface_name, []))


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
               idl_type == 'object' or
               idl_type == 'Promise')  # Promise will be basic in future


def sequence_type(idl_type):
    if is_union_type(idl_type):
        # We do not support sequences of union types
        return False
    matched = re.match(r'sequence<([\w\s]+)>', idl_type)
    return matched and matched.group(1)


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
    extended_attributes = None

    def resolve_typedefs(self, typedefs):
        """Resolve typedefs to actual types in the object."""
        # Constructors don't have their own return type, because it's the
        # interface itself.
        if not self.idl_type:
            return
        # (Types are represented either as strings or as IdlUnionType objects.)
        # Union types are objects, which have a member function for this
        if isinstance(self.idl_type, IdlUnionType):
            # Method 'resolve_typedefs' call is ok, but pylint can't infer this
            # pylint: disable=E1101
            self.idl_type.resolve_typedefs(typedefs)
            return
        # Otherwise, IDL type is represented as string, so use a function
        self.idl_type = resolve_typedefs(self.idl_type, typedefs)


class IdlType(object):
    # FIXME: replace type strings with these objects,
    # so don't need to parse everywhere types are used.
    # Types are stored internally as strings, not objects,
    # e.g., as 'sequence<Foo>' or 'Foo[]',
    # hence need to parse the string whenever a type is used.
    # FIXME: incorporate Nullable, Variadic, etc.
    # FIXME: properly should nest types
    # Formally types are nested, e.g., short?[] vs. short[]?,
    # but in practice these complex types aren't used and can treat
    # as orthogonal properties.
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

    @classmethod
    def from_string(cls, type_string):
        sequence_re = r'^sequence<([^>]*)>$'
        if type_string.endswith('[]'):
            type_string = type_string[:-2]
            sequence_match = re.match(sequence_re, type_string)
            if sequence_match:
                raise ValueError('Array of Sequences are not allowed.')
            return cls(type_string, is_array=True)
        sequence_match = re.match(sequence_re, type_string)
        if sequence_match:
            base_type = sequence_match.group(1)
            return cls(base_type, is_sequence=True)
        return cls(type_string)

    def resolve_typedefs(self, typedefs):
        if self.base_type in typedefs:
            self.base_type = typedefs[self.base_type]
        return self  # Fluent interface


class IdlUnionType(object):
    # FIXME: remove class, just treat as tuple?
    def __init__(self, union_member_types):
        self.union_member_types = union_member_types

    def resolve_typedefs(self, typedefs):
        self.union_member_types = [
            typedefs.get(union_member_type, union_member_type)
            for union_member_type in self.union_member_types]


def resolve_typedefs(idl_type, typedefs):
    """Return an IDL type (as string) with typedefs resolved."""
    # FIXME: merge into above, as only one use
    # Converts a string representation to and from an IdlType object to handle
    # parsing of composite types (arrays and sequences) and encapsulate typedef
    # resolution, e.g., GLint[] -> unsigned long[] requires parsing the '[]'.
    # Use fluent interface to avoid auxiliary variable.
    return str(IdlType.from_string(idl_type).resolve_typedefs(typedefs))
