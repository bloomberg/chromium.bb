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

"""Generate template values for an interface."""

import v8_attributes
from v8_utilities import cpp_class_name, runtime_enable_function_name, v8_class_name


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
    'core/platform/chromium/TraceEvent.h',
    'wtf/UnusedParam.h',
])


def generate_interface(interface):
    header_includes = INTERFACE_H_INCLUDES
    cpp_includes = INTERFACE_CPP_INCLUDES

    template_contents = {
        'interface_name': interface.name,
        'cpp_class_name': cpp_class_name(interface),
        'v8_class_name': v8_class_name(interface),
        'constants': [generate_constant(constant) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in interface.extended_attributes,
        # Includes are modified in-place after generating members
        'header_includes': header_includes,
        'cpp_includes': cpp_includes,
    }
    attributes_contents, attributes_includes = v8_attributes.generate_attributes(interface)
    template_contents.update(attributes_contents)
    cpp_includes |= attributes_includes
    return template_contents


def generate_constant(constant):
    # Extended Attributes: DeprecateAs, EnabledAtRuntime, Reflect
    # (Blink-only) string literals are unquoted in tokenizer, must be re-quoted
    # in C++.
    if constant.data_type == 'DOMString':
        value = '"%s"' % constant.value
    else:
        value = constant.value
    reflected_name = constant.extended_attributes.get('Reflect', constant.name)

    enabled_at_runtime = 'EnabledAtRuntime' in constant.extended_attributes
    constant_parameter = {
        'name': constant.name,
        # FIXME: use 'reflected_name' as correct 'name'
        'reflected_name': reflected_name,
        'value': value,
        'enabled_at_runtime': enabled_at_runtime,
        'runtime_enable_function_name': runtime_enable_function_name(constant) if enabled_at_runtime else None,
    }
    return constant_parameter
