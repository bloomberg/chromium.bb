# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Blink IDL Intermediate Representation (IR) classes.

Also JSON export, using legacy Perl terms and format, to ensure that both
parsers produce the same output.
FIXME: remove BaseIdl, JSON export (json_serializable and to_json), and Perl
compatibility functions and hacks once Perl compiler gone.
"""

# Disable attribute hiding check (else JSONEncoder default raises an error)
# pylint: disable=E0202
# pylint doesn't understand ABCs.
# pylint: disable=W0232, E0203, W0201

import abc
import json
import re


# Base classes


class BaseIdl(object):
    """Abstract base class, used for JSON serialization."""
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def json_serializable(self):
        """Returns a JSON serializable form of the object.

        This should be a dictionary, with keys scoped names of the form
        Class::key, where the scope is the class name.
        This is so we produce identical output to the Perl code, which uses
        the Perl module JSON.pm, which uses this format.
        """
        pass


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


# IDL classes


class IdlDefinitions(BaseIdl):
    def __init__(self, callback_functions=None, enumerations=None, file_name=None, interfaces=None, typedefs=None):
        self.callback_functions = callback_functions or {}
        self.enumerations = enumerations or {}
        self.file_name = file_name or None
        self.interfaces = interfaces or {}
        # Typedefs are not exposed by bindings; resolve typedefs with the
        # actual types and then discard the Typedefs.
        # http://www.w3.org/TR/WebIDL/#idl-typedefs
        if typedefs:
            self.resolve_typedefs(typedefs)

    def resolve_typedefs(self, typedefs):
        for callback_function in self.callback_functions.itervalues():
            callback_function.resolve_typedefs(typedefs)
        for interface in self.interfaces.itervalues():
            interface.resolve_typedefs(typedefs)

    def json_serializable(self):
        return {
                'idlDocument::callbackFunctions': self.callback_functions.values(),
                'idlDocument::enumerations': self.enumerations.values(),
                'idlDocument::fileName': self.file_name,
                'idlDocument::interfaces': sorted(self.interfaces.values()),
                }

    def to_json(self, debug=False):
        """Returns a JSON string representing the Definitions.

        The JSON output should be identical with the output of the Perl parser,
        specifically the function serializeJSON in idl_serializer.pm,
        which takes a Perl object created by idl_parser.pm.
        """
        # Sort so order consistent, allowing comparison of output
        if debug:
            # indent turns on pretty-printing for legibility
            return json.dumps(self, cls=IdlEncoder, sort_keys=True, indent=4)
        # Use compact separators so output identical to Perl
        return json.dumps(self, cls=IdlEncoder, sort_keys=True, separators=(',', ':'))


class IdlCallbackFunction(BaseIdl, TypedObject):
    def __init__(self, name=None, idl_type=None, arguments=None):
        self.idl_type = idl_type
        self.name = name
        self.arguments = arguments or []

    def resolve_typedefs(self, typedefs):
        TypedObject.resolve_typedefs(self, typedefs)
        for argument in self.arguments:
            argument.resolve_typedefs(typedefs)

    def json_serializable(self):
        return {
            'callbackFunction::name': self.name,
            'callbackFunction::type': self.idl_type,
            'callbackFunction::parameters': self.arguments,
            }


class IdlEnum(BaseIdl):
    def __init__(self, name=None, values=None):
        self.name = name
        self.values = values or []

    def json_serializable(self):
        return {
            'domEnum::name': self.name,
            'domEnum::values': self.values,
            }


class IdlInterface(BaseIdl):
    def __init__(self, attributes=None, constants=None, constructors=None, custom_constructors=None, extended_attributes=None, operations=None, is_callback=False, is_partial=False, name=None, parent=None):
        self.attributes = attributes or []
        self.constants = constants or []
        self.constructors = constructors or []
        self.custom_constructors = custom_constructors or []
        self.extended_attributes = extended_attributes or {}
        self.operations = operations or []
        self.is_callback = is_callback
        self.is_partial = is_partial
        self.is_exception = False
        self.name = name
        self.parent = parent

    def resolve_typedefs(self, typedefs):
        for attribute in self.attributes:
            attribute.resolve_typedefs(typedefs)
        for constant in self.constants:
            constant.resolve_typedefs(typedefs)
        for constructor in self.constructors:
            constructor.resolve_typedefs(typedefs)
        for custom_constructor in self.custom_constructors:
            custom_constructor.resolve_typedefs(typedefs)
        for operation in self.operations:
            operation.resolve_typedefs(typedefs)

    def json_serializable(self):
        return {
            'domInterface::attributes': self.attributes,
            'domInterface::constants': self.constants,
            'domInterface::constructors': self.constructors,
            'domInterface::customConstructors': self.custom_constructors,
            'domInterface::extendedAttributes': none_to_value_is_missing(self.extended_attributes),
            'domInterface::functions': self.operations,
            'domInterface::isException': false_to_none(self.is_exception),
            'domInterface::isCallback': boolean_to_perl(false_to_none(self.is_callback)),
            'domInterface::isPartial': false_to_none(self.is_partial),
            'domInterface::name': self.name,
            'domInterface::parent': self.parent,
            }


class IdlException(IdlInterface):
    # Properly exceptions and interfaces are distinct, and thus should inherit a
    # common base class (say, "IdlExceptionOrInterface").
    # However, there is only one exception (DOMException), and new exceptions
    # are not expected. Thus it is easier to implement exceptions as a
    # restricted subclass of interfaces.
    # http://www.w3.org/TR/WebIDL/#idl-exceptions
    def __init__(self, name=None, constants=None, operations=None, attributes=None, extended_attributes=None):
        IdlInterface.__init__(self, name=name, constants=constants, operations=operations, attributes=attributes, extended_attributes=extended_attributes)
        self.is_exception = True


class IdlAttribute(BaseIdl, TypedObject):
    def __init__(self, idl_type=None, extended_attributes=None, getter_exceptions=None, is_nullable=False, is_static=False, is_read_only=False, name=None, setter_exceptions=None):
        self.idl_type = idl_type
        self.extended_attributes = extended_attributes or {}
        self.getter_exceptions = getter_exceptions or []
        self.is_nullable = is_nullable
        self.is_static = is_static
        self.is_read_only = is_read_only
        self.name = name
        self.setter_exceptions = setter_exceptions or []

    def json_serializable(self):
        return {
            'domAttribute::extendedAttributes': none_to_value_is_missing(self.extended_attributes),
            'domAttribute::getterExceptions': self.getter_exceptions,
            'domAttribute::isNullable': boolean_to_perl_quoted(false_to_none(self.is_nullable)),
            'domAttribute::isReadOnly': boolean_to_perl(false_to_none(self.is_read_only)),
            'domAttribute::isStatic': boolean_to_perl(false_to_none(self.is_static)),
            'domAttribute::name': self.name,
            'domAttribute::setterExceptions': self.setter_exceptions,
            'domAttribute::type': self.idl_type,
            }


class IdlConstant(BaseIdl, TypedObject):
    def __init__(self, name=None, idl_type=None, value=None, extended_attributes=None):
        self.idl_type = idl_type
        self.extended_attributes = extended_attributes or {}
        self.name = name
        self.value = value

    def json_serializable(self):
        return {
            'domConstant::extendedAttributes': none_to_value_is_missing(self.extended_attributes),
            'domConstant::name': self.name,
            'domConstant::type': self.idl_type,
            'domConstant::value': self.value,
            }


class IdlOperation(BaseIdl, TypedObject):
    def __init__(self, is_static=False, name=None, idl_type=None, extended_attributes=None, specials=None, arguments=None, overloaded_index=None):
        self.is_static = is_static
        self.name = name or ''
        self.idl_type = idl_type
        self.extended_attributes = extended_attributes or {}
        self.specials = specials or []
        self.arguments = arguments or []
        # FIXME: remove overloaded_index (only here for Perl compatibility),
        # as overloading is handled in code generator (v8_interface.py).
        self.overloaded_index = overloaded_index

    def resolve_typedefs(self, typedefs):
        TypedObject.resolve_typedefs(self, typedefs)
        for argument in self.arguments:
            argument.resolve_typedefs(typedefs)

    def json_serializable(self):
        return {
            'domFunction::extendedAttributes': none_to_value_is_missing(self.extended_attributes),
            'domFunction::isStatic': boolean_to_perl(false_to_none(self.is_static)),
            'domFunction::name': self.name,
            'domFunction::overloadedIndex': self.overloaded_index,
            'domFunction::parameters': self.arguments,
            'domFunction::specials': self.specials,
            'domFunction::type': self.idl_type,
            }


class IdlArgument(BaseIdl, TypedObject):
    def __init__(self, name=None, idl_type=None, extended_attributes=None, is_optional=False, is_nullable=None, is_variadic=False):
        self.idl_type = idl_type
        self.extended_attributes = extended_attributes or {}
        # FIXME: boolean values are inconsistent.
        # The below hack is so that generated JSON is identical to
        # Perl-generated JSON, where the exact values depend on the code path.
        # False and None (Perl: 0 and undef) are semantically interchangeable,
        # but yield different JSON.
        # Once Perl removed, have all default to False.
        if is_optional is None:
            is_optional = False
            if is_variadic is None:
                is_variadic = False
        self.is_nullable = is_nullable  # (T?)
        self.is_optional = is_optional  # (optional T)
        self.is_variadic = is_variadic  # (T...)
        self.name = name

    def json_serializable(self):
        return {
            'domParameter::extendedAttributes': none_to_value_is_missing(self.extended_attributes),
            'domParameter::isNullable': boolean_to_perl_quoted(self.is_nullable),
            'domParameter::isOptional': boolean_to_perl(self.is_optional),
            'domParameter::isVariadic': boolean_to_perl(self.is_variadic),
            'domParameter::name': self.name,
            'domParameter::type': self.idl_type,
            }

# Type classes


def resolve_typedefs(idl_type, typedefs):
    """Return an IDL type (as string) with typedefs resolved."""
    # Converts a string representation to and from an IdlType object to handle
    # parsing of composite types (arrays and sequences) and encapsulate typedef
    # resolution, e.g., GLint[] -> unsigned long[] requires parsing the '[]'.
    # Use fluent interface to avoid auxiliary variable.
    return str(IdlType.from_string(idl_type).resolve_typedefs(typedefs))


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


class IdlUnionType(BaseIdl):
    def __init__(self, union_member_types=None):
        self.union_member_types = union_member_types or []

    def resolve_typedefs(self, typedefs):
        self.union_member_types = [
            typedefs.get(union_member_type, union_member_type)
            for union_member_type in self.union_member_types]

    def json_serializable(self):
        return {
            'UnionType::unionMemberTypes': self.union_member_types,
            }


# Perl JSON compatibility functions

def none_to_value_is_missing(extended_attributes):
    # Perl IDL Parser uses 'VALUE_IS_MISSING' for null values in
    # extended attributes, so add this as a filter when exporting to JSON.
    new_extended_attributes = {}
    for key, value in extended_attributes.iteritems():
        if value is None:
            new_extended_attributes[key] = 'VALUE_IS_MISSING'
        else:
            new_extended_attributes[key] = value
    return new_extended_attributes


def boolean_to_perl(value):
    # Perl stores booleans as 1, 0, or undefined (JSON null);
    # convert to this format.
    if value is None:
        return None
    return int(value)


def boolean_to_perl_quoted(value):
    # Bug-for-bug compatibility with Perl.
    # The value of isNullable is quoted ('1', '0', or undefined), rather than
    # an integer, so add quotes.
    if value is None:
        return None
    return str(int(value))


def false_to_none(value):
    # The Perl parser generally uses undefined (Python None) rather than False
    # for boolean flags, because the value is simply left undefined, rather than
    # explicitly set to False.
    if value is False:
        return None
    return value


# JSON export


class IdlEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, BaseIdl):
            return obj.json_serializable()
        return json.JSONEncoder.default(self, obj)
