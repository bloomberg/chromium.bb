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

"""Generate template values for an interface.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import v8_attributes
from v8_globals import includes
import v8_methods
import v8_utilities
from v8_utilities import cpp_name, runtime_enabled_function_name


INTERFACE_H_INCLUDES = set([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMWrapper.h',  # FIXME: necessary?
    'bindings/v8/WrapperTypeInfo.h',  # FIXME: necessary?
])
INTERFACE_CPP_INCLUDES = set([
    'RuntimeEnabledFeatures.h',
    'bindings/v8/ScriptController.h',
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMConfiguration.h',  # FIXME: necessary?
    'bindings/v8/V8DOMWrapper.h',  # FIXME: necessary?
    'core/dom/ContextFeatures.h',
    'core/dom/Document.h',
    'platform/TraceEvent.h',
    'wtf/UnusedParam.h',
])


def generate_interface(interface):
    includes.clear()
    includes.update(INTERFACE_CPP_INCLUDES)
    v8_class_name = v8_utilities.v8_class_name(interface)

    template_contents = {
        'cpp_class_name': cpp_name(interface),
        'header_includes': INTERFACE_H_INCLUDES,
        'interface_name': interface.name,
        'v8_class_name': v8_class_name,
    }

    template_contents.update({
        'constants': [generate_constant(constant) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in interface.extended_attributes,
    })

    attributes = [v8_attributes.generate_attribute(interface, attribute)
                  for attribute in interface.attributes]
    template_contents.update({
        'attributes': attributes,
        'has_constructor_attributes': any(attribute['is_constructor'] for attribute in attributes),
        'has_per_context_enabled_attributes': any(attribute['per_context_enabled_function_name'] for attribute in attributes),
        'has_replaceable_attributes': any(attribute['is_replaceable'] for attribute in attributes),
        'has_runtime_enabled_attributes': any(attribute['runtime_enabled_function_name'] for attribute in attributes),
    })

    methods = [v8_methods.generate_method(interface, method)
               for method in interface.operations]
    generate_overloads(methods)
    template_contents.update({
        'has_non_per_context_enabled_methods': any(not method['per_context_enabled_function_name'] for method in methods),
        'has_per_context_enabled_methods': any(method['per_context_enabled_function_name'] for method in methods),
        'methods': methods,
    })

    return template_contents


# [DeprecateAs], [Reflect], [RuntimeEnabled]
def generate_constant(constant):
    # (Blink-only) string literals are unquoted in tokenizer, must be re-quoted
    # in C++.
    if constant.idl_type == 'DOMString':
        value = '"%s"' % constant.value
    else:
        value = constant.value

    constant_parameter = {
        'name': constant.name,
        # FIXME: use 'reflected_name' as correct 'name'
        'reflected_name': constant.extended_attributes.get('Reflect', constant.name),
        'runtime_enabled_function_name': runtime_enabled_function_name(constant),
        'value': value,
    }
    return constant_parameter


def generate_overloads(methods):
    generate_overloads_by_type(methods, is_static=False)  # Regular methods
    generate_overloads_by_type(methods, is_static=True)


def generate_overloads_by_type(methods, is_static):
    # Generates |overloads| template values and modifies |methods| in place;
    # |is_static| flag used (instead of partitioning list in 2) because need to
    # iterate over original list of methods to modify in place
    method_counts = {}
    for method in methods:
        if method['is_static'] != is_static:
            continue
        name = method['name']
        method_counts.setdefault(name, 0)
        method_counts[name] += 1

    # Filter to only methods that are actually overloaded
    overloaded_method_counts = dict((name, count)
                                    for name, count in method_counts.iteritems()
                                    if count > 1)

    # Add overload information only to overloaded methods, so template code can
    # easily verify if a function is overloaded
    method_overloads = {}
    for method in methods:
        name = method['name']
        if (method['is_static'] != is_static or
            name not in overloaded_method_counts):
            continue
        # Overload index includes self, so first append, then compute index
        method_overloads.setdefault(name, []).append(method)
        method.update({
            'overload_index': len(method_overloads[name]),
            'overload_resolution_expression': overload_resolution_expression(method),
        })

    # Resolution function is generated after last overloaded function;
    # package necessary information into |method.overloads| for that method.
    for method in methods:
        if (method['is_static'] != is_static or
            'overload_index' not in method):
            continue
        name = method['name']
        if method['overload_index'] != overloaded_method_counts[name]:
            continue
        overloads = method_overloads[name]
        method['overloads'] = {
            'name': name,
            'methods': overloads,
            'minimum_number_of_required_arguments': min(
                overload['number_of_required_arguments']
                for overload in overloads),
        }


def overload_resolution_expression(method):
    # Expression is an OR of ANDs: each term in the OR corresponds to a
    # possible argument count for a given method, with type checks.
    # FIXME: Blink's overload resolution algorithm is incorrect, per:
    # https://code.google.com/p/chromium/issues/detail?id=293561
    arguments = method['arguments']
    checks = [overload_check_expression(method, index)
              # check *omitting* optional arguments at |index| and up, e.g.:
              # index 0 => argument_count 0 (no arguments)
              # index 1 => argument_count 1 (index 0 argument only)
              for index, argument in enumerate(arguments)
              if argument['is_optional']]
    # FIXME: this is wrong if a method has optional arguments and a variadic
    # one, though there are not yet any examples of this
    if not method['is_variadic']:
        # Includes all optional arguments (len = last index + 1)
        checks.append(overload_check_expression(method, len(arguments)))
    return ' || '.join(checks)


def overload_check_expression(method, argument_count):
    # FIXME: add type checks per GenerateParametersCheckExpression
    return 'info.Length() == %s' % argument_count
