# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template values for a callback function.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from utilities import to_snake_case
from v8_globals import includes

CALLBACK_FUNCTION_H_INCLUDES = frozenset([
    'bindings/core/v8/NativeValueTraits.h',
    'platform/bindings/CallbackFunctionBase.h',
    'platform/bindings/ScriptWrappable.h',
    'platform/bindings/TraceWrapperV8Reference.h',
    'platform/heap/Handle.h',
    'platform/wtf/text/WTFString.h',
])
CALLBACK_FUNCTION_CPP_INCLUDES = frozenset([
    'bindings/core/v8/ExceptionState.h',
    'platform/bindings/ScriptState.h',
    'bindings/core/v8/NativeValueTraitsImpl.h',
    'bindings/core/v8/ToV8ForCore.h',
    'bindings/core/v8/V8BindingForCore.h',
    'core/dom/ExecutionContext.h',
    'platform/wtf/Assertions.h',
])


def callback_function_context(callback_function):
    includes.clear()
    includes.update(CALLBACK_FUNCTION_CPP_INCLUDES)
    idl_type = callback_function.idl_type
    idl_type_str = str(idl_type)

    for argument in callback_function.arguments:
        argument.idl_type.add_includes_for_type(
            callback_function.extended_attributes)

    context = {
        # While both |callback_function_name| and |cpp_class| are identical at
        # the moment, the two are being defined because their values may change
        # in the future (e.g. if we support [ImplementedAs=] in callback
        # functions).
        'callback_function_name': callback_function.name,
        'cpp_class': 'V8%s' % callback_function.name,
        'cpp_includes': sorted(includes),
        'forward_declarations': sorted(forward_declarations(callback_function)),
        'header_includes': sorted(CALLBACK_FUNCTION_H_INCLUDES),
        'idl_type': idl_type_str,
        'this_include_header_name': to_snake_case('V8%s' % callback_function.name),
    }

    if idl_type_str != 'void':
        context.update({
            'return_cpp_type': idl_type.cpp_type + '&',
            'return_value': idl_type.v8_value_to_local_cpp_value(
                callback_function.extended_attributes,
                'v8ReturnValue', 'cppValue',
                isolate='script_state_->GetIsolate()',
                bailout_return_value='false'),
        })

    context.update(arguments_context(callback_function.arguments, context.get('return_cpp_type')))
    return context


def forward_declarations(callback_function):
    def find_forward_declaration(idl_type):
        if idl_type.is_interface_type or idl_type.is_dictionary:
            return idl_type.implemented_as
        elif idl_type.is_array_or_sequence_type:
            return find_forward_declaration(idl_type.element_type)
        return None

    declarations = []
    for argument in callback_function.arguments:
        name = find_forward_declaration(argument.idl_type)
        if name:
            declarations.append(name)
    return declarations


def arguments_context(arguments, return_cpp_type):
    def argument_context(argument):
        idl_type = argument.idl_type
        return {
            'cpp_value_to_v8_value': idl_type.cpp_value_to_v8_value(
                argument.name, isolate='script_state_->GetIsolate()',
                creation_context='script_state_->GetContext()->Global()'),
            'enum_type': idl_type.enum_type,
            'enum_values': idl_type.enum_values,
            'name': argument.name,
            'v8_name': 'v8_%s' % argument.name,
        }

    argument_declarations = [
        'ScriptWrappable* scriptWrappable',
    ]
    argument_declarations.extend(
        '%s %s' % (argument.idl_type.callback_cpp_type, argument.name)
        for argument in arguments)
    if return_cpp_type:
        argument_declarations.append('%s returnValue' % return_cpp_type)
    return {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
