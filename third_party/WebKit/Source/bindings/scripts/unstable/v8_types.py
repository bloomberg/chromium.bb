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

"""Functions for type handling and type conversion (Blink/C++ <-> V8/JS).

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import re

CPP_TYPE_SAME_AS_IDL_TYPE = set([
    'double',
    'float',
    'int',  # FIXME: int is not an IDL type
    'long long',
    'unsigned long long',
])
CPP_INT_TYPES = set([
    'byte',
    'long',
    'short',
])
CPP_UNSIGNED_TYPES = set([
    'octet',
    'unsigned int',
    'unsigned long',
    'unsigned short',
])
# Promise is not yet in the Web IDL spec but is going to be speced
# as primitive types in the future.
# Since V8 dosn't provide Promise primitive object currently,
# PRIMITIVE_TYPES doesn't contain Promise.
CPP_TYPE_SPECIAL_CONVERSION_RULES = {
    'boolean': 'bool',
    'DOMString': 'const String&',
    'Promise': 'ScriptPromise',
}
PRIMITIVE_TYPES = set([
     # http://www.w3.org/TR/WebIDL/#dfn-primitive-type
    'boolean',
    'float',
    # unrestricted float is not supported
    'double',
    # unrestricted double is not supported
    # integer types
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
    # Blink-specific additions
    'Date',
    'void',
])
CPP_VALUE_TO_V8_VALUE_DICT = {
    'boolean': 'v8Boolean({cpp_value}, {isolate})',
    # long long and unsigned long long are not representable in ECMAScript.
    'long long': 'v8::Number::New({isolate}, static_cast<double>({cpp_value}))',
    'unsigned long long': 'v8::Number::New({isolate}, static_cast<double>({cpp_value}))',
    'float': 'v8::Number::New({isolate}, {cpp_value})',
    'double': 'v8::Number::New({isolate}, {cpp_value})',
    'DOMTimeStamp': 'v8::Number::New({isolate}, static_cast<double>({cpp_value}))',
    'DOMString': 'v8String({cpp_value}, {isolate})',
}
CPP_VALUE_TO_V8_VALUE_ARRAY_OR_SEQUENCE_TYPE = 'v8Array({cpp_value}, {isolate})'
CPP_VALUE_TO_V8_VALUE_DEFAULT = 'toV8({cpp_value}, {creation_context}, {isolate})'
V8_SET_RETURN_VALUE_DICT = {
    'unsigned': 'v8SetReturnValueUnsigned({callback_info}, {cpp_value});',
}


def array_or_sequence_type(idl_type):
    return array_type(idl_type) or sequence_type(idl_type)


def array_type(idl_type):
    matched = re.match(r'([\w\s]+)\[\]', idl_type)
    return matched and matched.group(1)


def primitive_type(idl_type):
    return idl_type in PRIMITIVE_TYPES


def sequence_type(idl_type):
    matched = re.match(r'sequence<([\w\s]+)>', idl_type)
    return matched and matched.group(1)


def v8_type(interface_type):
    return 'V8' + interface_type


def cpp_type(idl_type, pointer_type=None):
    """Returns the C++ type corresponding to the IDL type.

    If unidentified, fall back to a pointer.

    Args:
        idl_type: IDL type
        pointer_type: If specified, return 'pointer_type<idl_type>'
            (e.g. RefPtr<Foo>, PassRefPtr<Foo>)
            else defaults to returning raw pointer form (e.g. 'Foo*').
    """
    if idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return idl_type
    if idl_type in CPP_INT_TYPES:
        return 'int'
    if idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if idl_type in CPP_TYPE_SPECIAL_CONVERSION_RULES:
        return CPP_TYPE_SPECIAL_CONVERSION_RULES[idl_type]
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return 'const Vector<{cpp_type} >&'.format(cpp_type=cpp_type(this_array_or_sequence_type, 'RefPtr'))
    if pointer_type:
        return '{pointer_type}<{idl_type}>'.format(pointer_type=pointer_type, idl_type=idl_type)
    return idl_type + '*'  # raw pointer


def cpp_value_to_v8_value(idl_type, cpp_value, isolate, creation_context=''):
    """Returns an expression converting a C++ value to a V8 value."""
    if idl_type in CPP_VALUE_TO_V8_VALUE_DICT:
        expression_format_string = CPP_VALUE_TO_V8_VALUE_DICT[idl_type]
    elif primitive_type(idl_type):  # primitive but not in dict
        raise Exception('unexpected IDL type %s' % idl_type)
    elif array_or_sequence_type(idl_type):
        expression_format_string = CPP_VALUE_TO_V8_VALUE_ARRAY_OR_SEQUENCE_TYPE
    else:
        expression_format_string = CPP_VALUE_TO_V8_VALUE_DEFAULT
    return expression_format_string.format(cpp_value=cpp_value, creation_context=creation_context, isolate=isolate)


def v8_set_return_value(idl_type, cpp_value, callback_info=''):
    """Returns a statement converting a C++ value to a V8 value and setting it as a return value."""
    this_cpp_type = cpp_type(idl_type)
    if this_cpp_type in V8_SET_RETURN_VALUE_DICT:
        expression_format_string = V8_SET_RETURN_VALUE_DICT[this_cpp_type]
    else:
        raise Exception('unexpected IDL type %s' % idl_type)
    return expression_format_string.format(callback_info=callback_info, cpp_value=cpp_value)


def includes_for_type(idl_type):
    if primitive_type(idl_type) or idl_type == 'DOMString':
        return set()
    if idl_type == 'Promise':
        return set(['ScriptPromise.h'])
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return includes_for_type(this_array_or_sequence_type)
    return set(['V8%s.h' % idl_type])
