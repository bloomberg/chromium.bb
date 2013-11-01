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

from v8_globals import includes
import v8_types
import v8_utilities
from v8_utilities import capitalize, cpp_name, has_extended_attribute, uncapitalize


def generate_attribute(interface, attribute):
    idl_type = attribute.idl_type
    extended_attributes = attribute.extended_attributes

    has_custom_getter = has_extended_attribute(attribute, ('Custom', 'CustomGetter'))
    has_custom_setter = not attribute.is_read_only and has_extended_attribute(attribute, ('Custom', 'CustomSetter'))
    contents = {
        'access_control_list': access_control_list(attribute),
        'activity_logging_world_list_for_getter': v8_utilities.activity_logging_world_list(attribute, 'Getter'),  # [ActivityLogging]
        'activity_logging_world_list_for_setter': v8_utilities.activity_logging_world_list(attribute, 'Setter'),  # [ActivityLogging]
        'cached_attribute_validation_method': extended_attributes.get('CachedAttribute'),
        'conditional_string': v8_utilities.generate_conditional_string(attribute),
        'cpp_type': v8_types.cpp_type(idl_type),
        'getter_callback_name': getter_callback_name(interface, attribute),
        'getter_callback_name_for_main_world': getter_callback_name_for_main_world(interface, attribute),
        'has_custom_getter': has_custom_getter,
        'has_custom_setter': has_custom_setter,
        'idl_type': idl_type,
        'is_constructor': is_constructor_attribute(attribute),
        'is_getter_raises_exception': has_extended_attribute(attribute, ('GetterRaisesException', 'RaisesException')),
        'is_keep_alive_for_gc': is_keep_alive_for_gc(attribute),
        'is_nullable': attribute.is_nullable,
        'is_read_only': attribute.is_read_only,
        'is_replaceable': 'Replaceable' in attribute.extended_attributes,
        'is_setter_raises_exception': has_extended_attribute(attribute, ('RaisesException', 'SetterRaisesException')),
        'is_static': attribute.is_static,
        'name': attribute.name,
        'per_context_enabled_function_name': v8_utilities.per_context_enabled_function_name(attribute),  # [PerContextEnabled]
        'property_attributes': property_attributes(attribute),
        'setter_callback_name': setter_callback_name(interface, attribute),
        'setter_callback_name_for_main_world': setter_callback_name_for_main_world(interface, attribute),
        'v8_type': v8_types.v8_type(idl_type),
        'runtime_enabled_function_name': v8_utilities.runtime_enabled_function_name(attribute),  # [RuntimeEnabled]
        'world_suffixes': ['', 'ForMainWorld'] if 'PerWorldBindings' in extended_attributes else [''],  # [PerWorldBindings]
        'wrapper_type_info': wrapper_type_info(attribute),
    }
    if is_constructor_attribute(attribute):
        includes.update(v8_types.includes_for_type(idl_type))
        return contents
    if not has_custom_getter:
        generate_getter(interface, attribute, contents)
    if not attribute.is_read_only and not has_custom_setter:
        generate_setter(interface, attribute, contents)

    return contents


# Getter

def generate_getter(interface, attribute, contents):
    idl_type = attribute.idl_type
    v8_types.add_includes_for_type(idl_type)
    extended_attributes = attribute.extended_attributes

    cpp_value = getter_expression(interface, attribute, contents)
    # Normally we can inline the function call into the return statement to
    # avoid the overhead of using a Ref<> temporary, but for some cases
    # (nullable types, EventHandler, [CachedAttribute], or if there are
    # exceptions), we need to use a local variable.
    # FIXME: check if compilers are smart enough to inline this, and if so,
    # always use a local variable (for readability and CG simplicity).
    if (attribute.is_nullable or
        idl_type == 'EventHandler' or
        'CachedAttribute' in extended_attributes or
        contents['is_getter_raises_exception']):
        contents['cpp_value_original'] = cpp_value
        cpp_value = 'jsValue'
    contents['cpp_value'] = cpp_value

    if contents['is_keep_alive_for_gc']:
        v8_set_return_value_statement = 'v8SetReturnValue(info, wrapper)'
        includes.add('bindings/v8/V8HiddenPropertyName.h')
    else:
        v8_set_return_value_statement = v8_types.v8_set_return_value(idl_type, cpp_value, extended_attributes=extended_attributes, script_wrappable='imp')
    contents['v8_set_return_value'] = v8_set_return_value_statement

    if (idl_type == 'EventHandler' and
        interface.name in ['Window', 'WorkerGlobalScope'] and
        attribute.name == 'onerror'):
        includes.add('bindings/v8/V8ErrorHandler.h')

    # [CheckSecurityForNode]
    is_check_security_for_node = 'CheckSecurityForNode' in extended_attributes
    if is_check_security_for_node:
        includes.add('bindings/v8/BindingSecurity.h')
    if is_check_security_for_node or contents['is_getter_raises_exception']:
        includes.update(set(['bindings/v8/ExceptionMessages.h',
                             'bindings/v8/ExceptionState.h']))
    v8_utilities.generate_deprecate_as(attribute, contents)  # [DeprecateAs]
    v8_utilities.generate_measure_as(attribute, contents)  # [MeasureAs]

    contents.update({
        'is_check_security_for_node': is_check_security_for_node,
        'is_unforgeable': 'Unforgeable' in extended_attributes,
    })


def getter_expression(interface, attribute, contents):
    arguments = []
    this_getter_base_name = getter_base_name(attribute, arguments)
    getter_name = scoped_name(interface, attribute, this_getter_base_name)

    call_with = attribute.extended_attributes.get('CallWith')
    arguments.extend(v8_utilities.call_with_arguments(call_with, contents))
    if attribute.is_nullable:
        arguments.append('isNull')
    if contents['is_getter_raises_exception']:
        arguments.append('es')
    if attribute.idl_type == 'EventHandler':
        arguments.append('isolatedWorldForIsolate(info.GetIsolate())')
    return '%s(%s)' % (getter_name, ', '.join(arguments))


CONTENT_ATTRIBUTE_GETTER_NAMES = {
    'boolean': 'fastHasAttribute',
    'long': 'getIntegralAttribute',
    'unsigned long': 'getUnsignedIntegralAttribute',
}


def getter_base_name(attribute, arguments):
    extended_attributes = attribute.extended_attributes
    if 'Reflect' not in extended_attributes:
        return uncapitalize(cpp_name(attribute))

    content_attribute_name = extended_attributes['Reflect'] or attribute.name.lower()
    if content_attribute_name in ['class', 'id', 'name']:
        # Special-case for performance optimization.
        return 'get%sAttribute' % content_attribute_name.capitalize()

    arguments.append(scoped_content_attribute_name(attribute))

    idl_type = attribute.idl_type
    if idl_type in CONTENT_ATTRIBUTE_GETTER_NAMES:
        return CONTENT_ATTRIBUTE_GETTER_NAMES[idl_type]
    if 'URL' in attribute.extended_attributes:
        return 'getURLAttribute'
    return 'fastGetAttribute'


def is_keep_alive_for_gc(attribute):
    idl_type = attribute.idl_type
    extended_attributes = attribute.extended_attributes
    return (
        'KeepAttributeAliveForGC' in extended_attributes or
        # For readonly attributes, for performance reasons we keep the attribute
        # wrapper alive while the owner wrapper is alive, because the attribute
        # never changes.
        (attribute.is_read_only and
         v8_types.is_wrapper_type(idl_type) and
         # There are some exceptions, however:
         not(
             # Node lifetime is managed by object grouping.
             v8_types.is_dom_node_type(idl_type) or
             # A self-reference is unnecessary.
             attribute.name == 'self' or
             # FIXME: Remove these hard-coded hacks.
             idl_type in ['EventHandler', 'Promise', 'Window'] or
             idl_type.startswith('HTML'))))


# Setter

def generate_setter(interface, attribute, contents):
    idl_type = attribute.idl_type
    extended_attributes = attribute.extended_attributes
    if v8_types.is_interface_type(idl_type) and not v8_types.array_type(idl_type):
        cpp_value = 'WTF::getPtr(cppValue)'
    else:
        cpp_value = 'cppValue'
    is_reflect = 'Reflect' in extended_attributes
    if is_reflect:
        includes.add('core/dom/custom/CustomElementCallbackDispatcher.h')
    contents.update({
        'cpp_setter': setter_expression(interface, attribute, contents),
        'enum_validation_expression': v8_utilities.enum_validation_expression(idl_type),
        'has_strict_type_checking': 'StrictTypeChecking' in extended_attributes and v8_types.is_interface_type(idl_type),
        'is_reflect': is_reflect,
        'v8_value_to_local_cpp_value': v8_types.v8_value_to_local_cpp_value(
            idl_type, extended_attributes, 'jsValue', 'cppValue'),
    })


def setter_expression(interface, attribute, contents):
    call_with = attribute.extended_attributes.get('SetterCallWith') or attribute.extended_attributes.get('CallWith')
    arguments = v8_utilities.call_with_arguments(call_with, contents)

    this_setter_base_name = setter_base_name(attribute, arguments)
    setter_name = scoped_name(interface, attribute, this_setter_base_name)

    idl_type = attribute.idl_type
    if idl_type == 'EventHandler':
        # FIXME: move V8EventListenerList.h to INCLUDES_FOR_TYPE
        includes.add('bindings/v8/V8EventListenerList.h')
        # FIXME: pass the isolate instead of the isolated world
        isolated_world = 'isolatedWorldForIsolate(info.GetIsolate())'
        arguments.extend(['V8EventListenerList::getEventListener(jsValue, true, ListenerFindOrCreate)', isolated_world])
        contents['event_handler_getter_expression'] = 'imp->%s(%s)' % (cpp_name(attribute), isolated_world)
    elif v8_types.is_interface_type(idl_type) and not v8_types.array_type(idl_type):
        # FIXME: should be able to eliminate WTF::getPtr in most or all cases
        arguments.append('WTF::getPtr(cppValue)')
    else:
        arguments.append('cppValue')
    if contents['is_setter_raises_exception']:
        arguments.append('es')

    return '%s(%s)' % (setter_name, ', '.join(arguments))


CONTENT_ATTRIBUTE_SETTER_NAMES = {
    'boolean': 'setBooleanAttribute',
    'long': 'setIntegralAttribute',
    'unsigned long': 'setUnsignedIntegralAttribute',
}


def setter_base_name(attribute, arguments):
    if 'Reflect' not in attribute.extended_attributes:
        return 'set%s' % capitalize(cpp_name(attribute))
    arguments.append(scoped_content_attribute_name(attribute))

    idl_type = attribute.idl_type
    if idl_type in CONTENT_ATTRIBUTE_SETTER_NAMES:
        return CONTENT_ATTRIBUTE_SETTER_NAMES[idl_type]
    return 'setAttribute'


def scoped_content_attribute_name(attribute):
    content_attribute_name = attribute.extended_attributes['Reflect'] or attribute.name.lower()
    namespace = 'HTMLNames'  # FIXME: can be SVG too
    includes.add('%s.h' % namespace)
    return '%s::%sAttr' % (namespace, content_attribute_name)


def scoped_name(interface, attribute, base_name):
    if attribute.is_static:
        return '%s::%s' % (interface.name, base_name)
    return 'imp->%s' % base_name


# Attribute configuration

def getter_callback_name(interface, attribute):
    if attribute.idl_type.endswith('Constructor'):
        return '{0}V8Internal::{0}ConstructorGetter'.format(cpp_name(interface))
    return '%sV8Internal::%sAttributeGetterCallback' % (cpp_name(interface), attribute.name)


# [Replaceable]
def setter_callback_name(interface, attribute):
    cpp_class_name = cpp_name(interface)
    if ('Replaceable' in attribute.extended_attributes or
        attribute.idl_type.endswith('Constructor')):
        # FIXME: rename to ForceSetAttributeOnThisCallback, since also used for Constructors
        return '{0}V8Internal::{0}ReplaceableAttributeSetterCallback'.format(cpp_class_name)
    # FIXME: support [PutForwards]
    if attribute.is_read_only:
        return '0'
    return '%sV8Internal::%sAttributeSetterCallback' % (cpp_class_name, attribute.name)


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


def wrapper_type_info(attribute):
    if not is_constructor_attribute(attribute):
        return '0'
    return 'const_cast<WrapperTypeInfo*>(&V8%s::wrapperTypeInfo)' % v8_types.constructor_type(attribute.idl_type)


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
    if ('NotEnumerable' in extended_attributes or
        is_constructor_attribute(attribute)):
        property_attributes_list.append('v8::DontEnum')
    if 'Unforgeable' in extended_attributes:
        property_attributes_list.append('v8::DontDelete')
    return property_attributes_list or ['v8::None']


def is_constructor_attribute(attribute):
    return attribute.idl_type.endswith('Constructor')
