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

"""Generate template values for methods.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import v8_types
import v8_utilities


def generate_method(method):
    arguments = method.arguments
    contents = {
        'arguments': [generate_argument(method, argument, index)
                      for index, argument in enumerate(arguments)],
        'cpp_method': cpp_method(method, len(arguments)),
        'custom_signature': custom_signature(arguments),
        'idl_type': method.idl_type,
        'name': method.name,
        'number_of_required_arguments': len([
            argument for argument in arguments
            if not (argument.is_optional or argument.is_variadic)]),
        'number_of_required_or_variadic_arguments': len([
            argument for argument in arguments
            if not argument.is_optional]),
    }
    return contents


def generate_argument(method, argument, index):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    return {
        'cpp_method': cpp_method(method, index),
        'cpp_type': v8_types.cpp_type(idl_type),
        'enum_validation_expression': v8_utilities.enum_validation_expression(idl_type),
        'has_default': 'Default' in extended_attributes,
        'idl_type': argument.idl_type,
        'index': index,
        'is_clamp': 'Clamp' in extended_attributes,
        'is_optional': argument.is_optional,
        'is_variadic_wrapper_type': argument.is_variadic and v8_types.is_wrapper_type(idl_type),
        'name': argument.name,
        'v8_value_to_local_cpp_value': v8_value_to_local_cpp_value(argument, index),
    }


def cpp_method(method, number_of_arguments):
    def cpp_argument(argument):
        if argument.idl_type in ['NodeFilter', 'XPathNSResolver']:
            # FIXME: remove this special case
            return '%s.get()' % argument.name
        return argument.name

    # Truncate omitted optional arguments
    arguments = method.arguments[:number_of_arguments]
    cpp_arguments = [cpp_argument(argument) for argument in arguments]
    cpp_value = 'imp->%s(%s)' % (method.name, ', '.join(cpp_arguments))

    idl_type = method.idl_type
    if idl_type == 'void':
        return cpp_value
    return v8_types.v8_set_return_value(idl_type, cpp_value)


def custom_signature(arguments):
    def argument_template(argument):
        idl_type = argument.idl_type
        if (v8_types.is_wrapper_type(idl_type) and
            not v8_types.is_typed_array_type(idl_type) and
            # Compatibility: all other browsers accepts a callable for
            # XPathNSResolver, despite it being against spec.
            not idl_type == 'XPathNSResolver'):
            return 'V8PerIsolateData::from(isolate)->rawTemplate(&V8{idl_type}::wrapperTypeInfo, currentWorldType)'.format(idl_type=idl_type)
        return 'v8::Handle<v8::FunctionTemplate>()'

    if (any(argument.is_optional and
            'Default' not in argument.extended_attributes
            for argument in arguments) or
        all(not v8_types.is_wrapper_type(argument.idl_type)
            for argument in arguments)):
        return None
    return ', '.join([argument_template(argument) for argument in arguments])


def v8_value_to_local_cpp_value(argument, index):
    extended_attributes = argument.extended_attributes
    idl_type = argument.idl_type
    name = argument.name
    if argument.is_variadic:
        return 'V8TRYCATCH_VOID(Vector<{cpp_type}>, {name}, toNativeArguments<{cpp_type}>(info, {index}))'.format(
                cpp_type=v8_types.cpp_type(idl_type), name=name, index=index)
    if (argument.is_optional and idl_type == 'DOMString' and
        extended_attributes.get('Default') == 'NullString'):
        v8_value = 'argumentOrNull(info, %s)' % index
    else:
        v8_value = 'info[%s]' % index
    return v8_types.v8_value_to_local_cpp_value(
        idl_type, argument.extended_attributes, v8_value, name, index=index)
