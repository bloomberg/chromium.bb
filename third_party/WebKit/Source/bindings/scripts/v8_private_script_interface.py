# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate template values for V8PrivateScriptInterface.

Blink-in-JS design doc: https://docs.google.com/a/google.com/document/d/13cT9Klgvt_ciAR3ONGvzKvw6fz9-f6E0FrqYFqfoc8Y/edit
"""

from idl_types import IdlType
from v8_globals import includes
import v8_types
import v8_utilities

BLINK_IN_JS_INTERFACE_H_INCLUDES = frozenset([
    'bindings/v8/V8Binding.h',
])

BLINK_IN_JS_INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/V8Window.h',
    'bindings/v8/PrivateScriptRunner.h',
    'core/dom/ScriptForbiddenScope.h',
    'core/frame/LocalFrame.h',
])


def private_script_interface_context(private_script_interface):
    includes.clear()
    includes.update(BLINK_IN_JS_INTERFACE_CPP_INCLUDES)
    return {
        'cpp_class': private_script_interface.name,
        'forward_declarations': forward_declarations(private_script_interface),
        'header_includes': set(BLINK_IN_JS_INTERFACE_H_INCLUDES),
        'methods': [method_context(operation)
                    for operation in private_script_interface.operations],
        'v8_class': v8_utilities.v8_class_name(private_script_interface),
    }


def forward_declarations(private_script_interface):
    declarations = set(['LocalFrame'])
    for operation in private_script_interface.operations:
        if operation.idl_type.is_wrapper_type:
            declarations.add(operation.idl_type.base_type)
        for argument in operation.arguments:
            if argument.idl_type.is_wrapper_type:
                declarations.add(argument.idl_type.base_type)
    return sorted(declarations)


def method_context(operation):
    extended_attributes = operation.extended_attributes
    idl_type = operation.idl_type
    idl_type_str = str(idl_type)
    contents = {
        'cpp_type': idl_type.cpp_type_args(raw_type=True),
        'name': operation.name,
        'v8_value_to_cpp_value': v8_types.v8_value_to_cpp_value(
            idl_type, extended_attributes, 'v8Value', 0,
            isolate='scriptState->isolate()'),
        }
    contents.update(arguments_context(operation.arguments, idl_type))
    return contents


def arguments_context(arguments, idl_type):
    def argument_context(argument):
        return {
            'handle': '%sHandle' % argument.name,
            'cpp_value_to_v8_value': argument.idl_type.cpp_value_to_v8_value(
                argument.name, isolate='scriptState->isolate()',
                creation_context='scriptState->context()->Global()'),
        }

    argument_declarations = ['LocalFrame* frame']
    argument_declarations.extend(['%s %s' % (argument.idl_type.cpp_type_args(
        used_as_argument=True), argument.name) for argument in arguments])
    if idl_type.name != 'void':
        argument_declarations.append('%s* %s' % (idl_type.cpp_type, 'output'))
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [argument_context(argument) for argument in arguments],
    }
