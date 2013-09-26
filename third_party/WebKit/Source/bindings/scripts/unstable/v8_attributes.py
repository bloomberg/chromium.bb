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

"""Generate template values for attributes.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import v8_types
import v8_utilities
from v8_utilities import cpp_method_name, generate_conditional_string, uncapitalize


def generate_attributes(interface):
    def generate_attribute(attribute):
        attribute_contents, attribute_includes = generate_attribute_and_includes(attribute)
        includes.update(attribute_includes)
        return attribute_contents

    includes = set()
    contents = generate_attributes_common(interface)
    contents['attributes'] = [generate_attribute(attribute) for attribute in interface.attributes]
    return contents, includes


def generate_attributes_common(interface):
    attributes = interface.attributes
    v8_class_name = v8_utilities.v8_class_name(interface)
    return {
        # Size 0 constant array is not allowed in VC++
        'number_of_attributes': 'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class_name if attributes else '0',
        'attribute_templates': '%sAttributes' % v8_class_name if attributes else '0',
    }


def generate_attribute_and_includes(attribute):
    idl_type = attribute.data_type
    this_should_keep_attribute_alive = should_keep_attribute_alive(attribute)
    contents = {
        'name': attribute.name,
        'conditional_string': generate_conditional_string(attribute),
        'cpp_method_name': cpp_method_name(attribute),
        'cpp_type': v8_types.cpp_type(idl_type),
        'should_keep_attribute_alive': this_should_keep_attribute_alive,
        'v8_type': v8_types.v8_type(idl_type),
    }
    if this_should_keep_attribute_alive:
        includes = v8_types.includes_for_type(idl_type)
        includes.add('bindings/v8/V8HiddenPropertyName.h')
    else:
        cpp_value = 'imp->%s()' % uncapitalize(attribute.name)
        return_v8_value_statement, includes = v8_types.v8_set_return_value(idl_type, cpp_value, callback_info='info', isolate='info.GetIsolate()', extended_attributes=attribute.extended_attributes)
        contents['return_v8_value_statement'] = return_v8_value_statement
    return contents, includes


def should_keep_attribute_alive(attribute):
    idl_type = attribute.data_type
    extended_attributes = attribute.extended_attributes
    return (
        'KeepAttributeAliveForGC' in extended_attributes or
        # For readonly attributes, for performance reasons we keep the attribute
        # wrapper alive while the owner wrapper is alive, because the attribute
        # never changes. (There are some exceptions.)
        (attribute.is_read_only and v8_types.wrapper_type(idl_type)))
