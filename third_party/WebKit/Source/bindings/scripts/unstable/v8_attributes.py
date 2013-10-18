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
from v8_utilities import capitalize, cpp_name, has_extended_attribute, uncapitalize


def generate_attributes(interface):
    def generate_attribute(attribute):
        attribute_contents, attribute_includes = generate_attribute_and_includes(interface, attribute)
        includes.update(attribute_includes)
        return attribute_contents

    includes = set()
    contents = generate_attributes_common(interface)
    contents['attributes'] = [generate_attribute(attribute) for attribute in interface.attributes]
    contents['has_per_context_enabled_attributes'] = any(attribute['per_context_enabled_function_name'] for attribute in contents['attributes'])
    contents['has_runtime_enabled_attributes'] = any(attribute['runtime_enabled_function_name'] for attribute in contents['attributes'])
    return contents, includes


def generate_attributes_common(interface):
    attributes = interface.attributes
    v8_class_name = v8_utilities.v8_class_name(interface)
    return {
        # Size 0 constant array is not allowed in VC++
        'number_of_attributes': 'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class_name if attributes else '0',
        'attribute_templates': '%sAttributes' % v8_class_name if attributes else '0',
    }


def generate_attribute_and_includes(interface, attribute):
    idl_type = attribute.data_type
    extended_attributes = attribute.extended_attributes
    this_is_keep_alive_for_gc = is_keep_alive_for_gc(attribute)
    has_setter = not attribute.is_read_only  # FIXME: need to check [Reflect] and [PutForwards]
    contents = {
        'access_control_list': access_control_list(attribute),
        'cached_attribute_validation_method': extended_attributes.get('CachedAttribute'),
        'conditional_string': v8_utilities.generate_conditional_string(attribute),
        'cpp_type': v8_types.cpp_type(idl_type),
        'getter_callback_name': getter_callback_name(interface, attribute),
        'getter_callback_name_for_main_world': getter_callback_name_for_main_world(interface, attribute),
        'idl_type': idl_type,
        'has_setter': has_setter,
        'is_keep_alive_for_gc': this_is_keep_alive_for_gc,
        'is_nullable': attribute.is_nullable,
        'is_static': attribute.is_static,
        'name': attribute.name,
        'per_context_enabled_function_name': v8_utilities.per_context_enabled_function_name(attribute),  # [PerContextEnabled]
        'property_attributes': property_attributes(attribute),
        'setter_callback_name': setter_callback_name(interface, attribute),
        'setter_callback_name_for_main_world': setter_callback_name_for_main_world(interface, attribute),
        'v8_type': v8_types.v8_type(idl_type),
        'runtime_enabled_function_name': v8_utilities.runtime_enabled_function_name(attribute),  # [RuntimeEnabled]
        'world_suffixes': ['', 'ForMainWorld'] if 'PerWorldBindings' in attribute.extended_attributes else [''],  # [PerWorldBindings]
    }
    if has_extended_attribute(attribute, ('Custom', 'CustomGetter')):
        contents['has_custom_getter'] = True
        return contents, set()

    includes = set()
    cpp_value = getter_expression(interface, attribute, contents, includes)
    # [GetterRaisesException], [RaisesException]
    is_getter_raises_exception = has_extended_attribute(attribute, ('GetterRaisesException', 'RaisesException'))
    # Normally we can inline the function call into the return statement to
    # avoid the overhead of using a Ref<> temporary, but for some cases
    # (nullable types, EventHandler, [CachedAttribute], or if there are
    # exceptions), we need to use a local variable.
    # FIXME: check if compilers are smart enough to inline this, and if so,
    # always use a local variable (for readability and CG simplicity).
    if (attribute.is_nullable or
        idl_type == 'EventHandler' or
        'CachedAttribute' in extended_attributes or
        is_getter_raises_exception):
        contents['cpp_value_original'] = cpp_value
        cpp_value = 'jsValue'
    contents['cpp_value'] = cpp_value

    if this_is_keep_alive_for_gc:
        v8_set_return_value_statement = 'v8SetReturnValue(info, wrapper)'
        includes.update(v8_types.includes_for_type(idl_type))
        includes.add('bindings/v8/V8HiddenPropertyName.h')
    else:
        v8_set_return_value_statement = v8_types.v8_set_return_value(idl_type, cpp_value, includes, callback_info='info', isolate='info.GetIsolate()', extended_attributes=extended_attributes, script_wrappable='imp')
    contents['v8_set_return_value'] = v8_set_return_value_statement

    if (idl_type == 'EventHandler' and
        interface.name in ['Window', 'WorkerGlobalScope'] and
        attribute.name == 'onerror'):
            includes.add('bindings/v8/V8ErrorHandler.h')

    # [CheckSecurityForNode]
    is_check_security_for_node = 'CheckSecurityForNode' in extended_attributes
    if is_check_security_for_node:
        includes.add('bindings/v8/BindingSecurity.h')
    v8_utilities.generate_measure_as(attribute, contents, includes)  # [MeasureAs]

    # [DeprecateAs]
    v8_utilities.generate_deprecate_as(attribute, contents, includes)
    if is_check_security_for_node or is_getter_raises_exception:
        includes.update(set(['bindings/v8/ExceptionMessages.h',
                             'bindings/v8/ExceptionState.h']))

    contents.update({
        'activity_logging_getter': v8_utilities.has_activity_logging(attribute, includes, 'Getter'),  # [ActivityLogging]
        'is_check_security_for_node': is_check_security_for_node,
        'is_getter_raises_exception': is_getter_raises_exception,
        'is_unforgeable': 'Unforgeable' in extended_attributes,
    })

    # Setter
    if not has_setter:
        return contents, includes

    if v8_types.interface_type(idl_type) and not v8_types.array_type(idl_type):
        cpp_value = 'WTF::getPtr(cppValue)'
    else:
        cpp_value = 'cppValue'
    contents.update({
        'v8_value_to_local_cpp_value': v8_types.v8_value_to_local_cpp_value(idl_type, attribute.extended_attributes, 'jsValue', 'cppValue', includes, 'info.GetIsolate()'),
        'cpp_setter': 'imp->set%s(%s)' % (capitalize(cpp_name(attribute)), cpp_value),
    })

    return contents, includes


def getter_expression(interface, attribute, contents, includes):
    arguments = []
    if 'Reflect' in attribute.extended_attributes:
        getter_base_name = content_attribute_getter_base_name(attribute, includes, arguments)
    else:
        getter_base_name = uncapitalize(cpp_name(attribute))

    if attribute.is_static:
        getter_name = '%s::%s' % (interface.name, getter_base_name)
    else:
        getter_name = 'imp->%s' % getter_base_name

    arguments.extend(v8_utilities.call_with_arguments(attribute, contents))
    if attribute.is_nullable:
        arguments.append('isNull')
    if has_extended_attribute(attribute, ('GetterRaisesException', 'RaisesException')):
        arguments.append('es')
    if attribute.data_type == 'EventHandler':
        arguments.append('isolatedWorldForIsolate(info.GetIsolate())')
    return '%s(%s)' % (getter_name, ', '.join(arguments))


CONTENT_ATTRIBUTE_GETTER_NAMES = {
    'boolean': 'fastHasAttribute',
    'long': 'getIntegralAttribute',
    'unsigned long': 'getUnsignedIntegralAttribute',
}


def content_attribute_getter_base_name(attribute, includes, arguments):
    content_attribute_name = attribute.extended_attributes['Reflect'] or attribute.name.lower()
    namespace = 'HTMLNames'  # FIXME: can be SVG too
    includes.add('%s.h' % namespace)

    if content_attribute_name in ['class', 'id', 'name']:
        # Special-case for performance optimization.
        return 'get%sAttribute' % content_attribute_name.capitalize()

    scoped_name = '%s::%sAttr' % (namespace, content_attribute_name)
    arguments.append(scoped_name)

    idl_type = attribute.data_type
    if idl_type in CONTENT_ATTRIBUTE_GETTER_NAMES:
        return CONTENT_ATTRIBUTE_GETTER_NAMES[idl_type]
    if 'URL' in attribute.extended_attributes:
        return 'getURLAttribute'
    return 'fastGetAttribute'


def is_keep_alive_for_gc(attribute):
    idl_type = attribute.data_type
    extended_attributes = attribute.extended_attributes
    return (
        'KeepAttributeAliveForGC' in extended_attributes or
        # For readonly attributes, for performance reasons we keep the attribute
        # wrapper alive while the owner wrapper is alive, because the attribute
        # never changes.
        (attribute.is_read_only and
         v8_types.wrapper_type(idl_type) and
         # There are some exceptions, however:
         not(
             # Node lifetime is managed by object grouping.
             v8_types.dom_node_type(idl_type) or
             # A self-reference is unnecessary.
             attribute.name == 'self' or
             # FIXME: Remove these hard-coded hacks.
             idl_type in ['EventHandler', 'Promise', 'Window'] or
             idl_type.startswith('HTML'))))


def getter_callback_name(interface, attribute):
    return '%sV8Internal::%sAttributeGetterCallback' % (cpp_name(interface), attribute.name)


def setter_callback_name(interface, attribute):
    if attribute.is_read_only:
        return '0'
    return '%sV8Internal::%sAttributeSetterCallback' % (cpp_name(interface), attribute.name)


# [DoNotCheckSecurity], [DoNotCheckSecurityOnGetter], [DoNotCheckSecurityOnSetter], [Unforgeable]
def access_control_list(attribute):
    extended_attributes = attribute.extended_attributes
    access_control = []
    if 'DoNotCheckSecurity' in extended_attributes:
        access_control.append('v8::ALL_CAN_READ')
        if not attribute.is_read_only:
            access_control.append('v8::ALL_CAN_WRITE')
    if 'DoNotCheckSecurityOnSetter' in extended_attributes:
        access_control.append('v8::ALL_CAN_WRITE')
    if 'DoNotCheckSecurityOnGetter' in extended_attributes:
        access_control.append('v8::ALL_CAN_READ')
    if 'Unforgeable' in extended_attributes:
        access_control.append('v8::PROHIBITS_OVERWRITING')
    return access_control or ['v8::DEFAULT']


# [NotEnumerable], [Unforgeable]
def property_attributes(attribute):
    extended_attributes = attribute.extended_attributes
    property_attributes_list = []
    if 'NotEnumerable' in extended_attributes:
        property_attributes_list.append('v8::DontEnum')
    if 'Unforgeable' in extended_attributes:
        property_attributes_list.append('v8::DontDelete')
    return property_attributes_list or ['v8::None']


# [PerWorldBindings]
def getter_callback_name_for_main_world(interface, attribute):
    if 'PerWorldBindings' not in attribute.extended_attributes:
        return '0'
    return '%sV8Internal::%sAttributeGetterCallbackForMainWorld' % (cpp_name(interface), attribute.name)


def setter_callback_name_for_main_world(interface, attribute):
    if ('PerWorldBindings' not in attribute.extended_attributes or
        attribute.is_read_only):
        return '0'
    return '%sV8Internal::%sAttributeSetterCallbackForMainWorld' % (cpp_name(interface), attribute.name)
