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

Extends IdlTypeBase with property |callback_cpp_type|.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from idl_types import IdlTypeBase
from v8_globals import includes
from v8_interface import constant_context
import v8_types
import v8_utilities

CALLBACK_INTERFACE_H_INCLUDES = frozenset([
    'platform/bindings/CallbackInterfaceBase.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/GeneratedCodeHelper.h',
    'bindings/core/v8/V8BindingForCore.h',
    'core/dom/ExecutionContext.h',
])
LEGACY_CALLBACK_INTERFACE_H_INCLUDES = frozenset([
    'platform/bindings/DOMWrapperWorld.h',
])
LEGACY_CALLBACK_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/V8BindingForCore.h',
    'bindings/core/v8/V8DOMConfiguration.h',
])


def cpp_type(idl_type):
    # FIXME: remove this function by making callback types consistent
    # (always use usual v8_types.cpp_type)
    idl_type_name = idl_type.name
    if idl_type_name == 'String' or idl_type.is_enum:
        return 'const String&'
    if idl_type_name == 'void':
        return 'void'
    # Callbacks use raw pointers, so raw_type=True
    raw_cpp_type = idl_type.cpp_type_args(raw_type=True)
    # Pass containers and dictionaries to callback method by const reference rather than by value
    if raw_cpp_type.startswith(('Vector', 'HeapVector')) or idl_type.is_dictionary:
        return 'const %s&' % raw_cpp_type
    return raw_cpp_type

IdlTypeBase.callback_cpp_type = property(cpp_type)


def callback_interface_context(callback_interface, _):
    includes.clear()
    includes.update(CALLBACK_INTERFACE_CPP_INCLUDES)

    # https://heycam.github.io/webidl/#dfn-single-operation-callback-interface
    is_single_operation = True
    if (callback_interface.parent or
            len(callback_interface.attributes) > 0 or
            len(callback_interface.operations) == 0):
        is_single_operation = False
    else:
        operations = callback_interface.operations
        basis = operations[0]
        for op in operations[1:]:
            if op.name != basis.name:
                is_single_operation = False
                break

    return {
        'cpp_class': callback_interface.name,
        'v8_class': v8_utilities.v8_class_name(callback_interface),
        'header_includes': set(CALLBACK_INTERFACE_H_INCLUDES),
        'is_single_operation_callback_interface': is_single_operation,
        'methods': [method_context(operation)
                    for operation in callback_interface.operations],
    }


def legacy_callback_interface_context(callback_interface, _):
    includes.clear()
    includes.update(LEGACY_CALLBACK_INTERFACE_CPP_INCLUDES)
    return {
        # TODO(bashi): Fix crbug.com/630986, and add 'methods'.
        'constants': [constant_context(constant, callback_interface)
                      for constant in callback_interface.constants],
        'cpp_class': callback_interface.name,
        'header_includes': set(LEGACY_CALLBACK_INTERFACE_H_INCLUDES),
        'interface_name': callback_interface.name,
        'v8_class': v8_utilities.v8_class_name(callback_interface),
    }


def add_includes_for_operation(operation):
    operation.idl_type.add_includes_for_type()
    for argument in operation.arguments:
        argument.idl_type.add_includes_for_type()


def method_context(operation):
    extended_attributes = operation.extended_attributes
    idl_type = operation.idl_type
    idl_type_str = str(idl_type)
    if idl_type_str not in ['boolean', 'void']:
        raise Exception('We only support callbacks that return boolean or void values.')
    is_custom = 'Custom' in extended_attributes
    if not is_custom:
        add_includes_for_operation(operation)
    call_with = extended_attributes.get('CallWith')
    call_with_this_handle = v8_utilities.extended_attribute_value_contains(call_with, 'ThisValue')
    context = {
        'call_with_this_handle': call_with_this_handle,
        'cpp_type': idl_type.callback_cpp_type,
        'idl_type': idl_type_str,
        'is_custom': is_custom,
        'name': operation.name,
    }
    context.update(arguments_context(operation.arguments,
                                     call_with_this_handle))
    return context


def arguments_context(arguments, call_with_this_handle):
    def argument_context(argument):
        return {
            'handle': '%sHandle' % argument.name,
            'cpp_value_to_v8_value': argument.idl_type.cpp_value_to_v8_value(
                argument.name, isolate='GetIsolate()',
                creation_context='argument_creation_context'),
        }

    argument_declarations = ['ScriptValue thisValue'] if call_with_this_handle else []
    argument_declarations.extend(
        '%s %s' % (argument.idl_type.callback_cpp_type, argument.name)
        for argument in arguments)
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
