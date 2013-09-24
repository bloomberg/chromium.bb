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

"""Generate template values for a callback interface.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

from v8_types import cpp_type, cpp_value_to_v8_value, includes_for_type
from v8_utilities import v8_class_name

CALLBACK_INTERFACE_H_INCLUDES = set([
    'bindings/v8/ActiveDOMCallback.h',
    'bindings/v8/DOMWrapperWorld.h',
    'bindings/v8/ScopedPersistent.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = set([
    'core/dom/ScriptExecutionContext.h',
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8Callback.h',
    'wtf/Assertions.h',
])
CPP_TO_V8_CONVERSION = 'v8::Handle<v8::Value> {name}Handle = {cpp_value_to_v8_value};'


def cpp_to_v8_conversion(idl_type, name):
    # Includes handled in includes_for_operation
    this_cpp_value_to_v8_value, _ = cpp_value_to_v8_value(idl_type, name, 'isolate', creation_context='v8::Handle<v8::Object>()')
    return CPP_TO_V8_CONVERSION.format(name=name, cpp_value_to_v8_value=this_cpp_value_to_v8_value)


def generate_callback_interface(callback_interface):
    cpp_includes = CALLBACK_INTERFACE_CPP_INCLUDES

    def generate_method(operation):
        method_contents, method_cpp_includes = generate_method_and_includes(operation)
        cpp_includes.update(method_cpp_includes)
        return method_contents

    methods = [generate_method(operation) for operation in callback_interface.operations]
    template_contents = {
        'cpp_class_name': callback_interface.name,
        'v8_class_name': v8_class_name(callback_interface),
        'header_includes': CALLBACK_INTERFACE_H_INCLUDES,
        'cpp_includes': cpp_includes,
        'methods': methods,
    }
    return template_contents


def generate_method_and_includes(operation):
    if 'Custom' in operation.extended_attributes:
        return generate_method_contents(operation), []
    if operation.data_type != 'boolean':
        raise Exception("We don't yet support callbacks that return non-boolean values.")
    cpp_includes = includes_for_operation(operation)
    return generate_method_contents(operation), cpp_includes


def includes_for_operation(operation):
    includes = includes_for_type(operation.data_type)
    for argument in operation.arguments:
        includes.update(includes_for_type(argument.data_type))
    return includes


def generate_method_contents(operation):
    contents = {
        'name': operation.name,
        'return_cpp_type': cpp_type(operation.data_type, 'RefPtr'),
        'custom': 'Custom' in operation.extended_attributes,
    }
    contents.update(generate_arguments_contents(operation.arguments))
    return contents


def generate_arguments_contents(arguments):
    def argument_declaration(argument):
        return '%s %s' % (cpp_type(argument.data_type), argument.name)

    def generate_argument(argument):
        return {
            'name': argument.name,
            'cpp_to_v8_conversion': cpp_to_v8_conversion(argument.data_type, argument.name),
        }

    return  {
        'argument_declarations': [argument_declaration(argument) for argument in arguments],
        'arguments': [generate_argument(argument) for argument in arguments],
        'handles': ['%sHandle' % argument.name for argument in arguments],
    }
