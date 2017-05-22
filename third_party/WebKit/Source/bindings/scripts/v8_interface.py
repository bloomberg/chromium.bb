# Copyright (C) 2013 Google Inc. All rights reserved.
# coding=utf-8
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

# pylint: disable=relative-import

"""Generate template values for an interface.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""
from operator import or_

from idl_definitions import IdlAttribute, IdlOperation, IdlArgument
from idl_types import IdlType, inherits_interface
from overload_set_algorithm import effective_overload_set_by_length
from overload_set_algorithm import method_overloads_by_name

import v8_attributes
from v8_globals import includes
import v8_methods
import v8_types
import v8_utilities
from v8_utilities import (cpp_name_or_partial, cpp_name, has_extended_attribute_value,
                          runtime_enabled_feature_name, is_legacy_interface_type_checking)


INTERFACE_H_INCLUDES = frozenset([
    'bindings/core/v8/GeneratedCodeHelper.h',
    'bindings/core/v8/NativeValueTraits.h',
    'platform/bindings/ScriptWrappable.h',
    'bindings/core/v8/ToV8ForCore.h',
    'bindings/core/v8/V8BindingForCore.h',
    'platform/bindings/V8DOMWrapper.h',
    'platform/bindings/WrapperTypeInfo.h',
    'platform/heap/Handle.h',
])
INTERFACE_CPP_INCLUDES = frozenset([
    'bindings/core/v8/ExceptionState.h',
    'bindings/core/v8/V8DOMConfiguration.h',
    'platform/bindings/V8ObjectConstructor.h',
    'core/dom/ExecutionContext.h',
    'platform/wtf/GetPtr.h',
    'platform/wtf/RefPtr.h',
])


def filter_has_constant_configuration(constants):
    return [constant for constant in constants if
            not constant['measure_as'] and
            not constant['deprecate_as'] and
            not constant['runtime_enabled_feature_name'] and
            not constant['origin_trial_feature_name']]


def filter_has_special_getter(constants):
    return [constant for constant in constants if
            constant['measure_as'] or
            constant['deprecate_as']]


def filter_runtime_enabled(constants):
    return [constant for constant in constants if
            constant['runtime_enabled_feature_name']]


def filter_origin_trial_enabled(constants):
    return [constant for constant in constants if
            constant['origin_trial_feature_name']]


def constant_filters():
    return {'has_constant_configuration': filter_has_constant_configuration,
            'has_special_getter': filter_has_special_getter,
            'runtime_enabled_constants': filter_runtime_enabled,
            'origin_trial_enabled_constants': filter_origin_trial_enabled}


def origin_trial_features(interface, constants, attributes, methods):
    """ Returns a list of the origin trial features used in this interface.

    Each element is a dictionary with keys 'name' and 'needs_instance'.
    'needs_instance' is true if any member associated with the interface needs
    to be installed on every instance of the interface. This list is the union
    of the sets of features used for constants, attributes and methods.
    """
    KEY = 'origin_trial_feature_name'  # pylint: disable=invalid-name

    def member_filter(members):
        return sorted([member for member in members if member.get(KEY) and not member.get('exposed_test')])

    def member_filter_by_name(members, name):
        return [member for member in members if member[KEY] == name]

    # Collect all members visible on this interface with a defined origin trial
    origin_trial_constants = member_filter(constants)
    origin_trial_attributes = member_filter(attributes)
    origin_trial_methods = member_filter([method for method in methods
                                          if v8_methods.method_is_visible(method, interface.is_partial) and
                                          not v8_methods.custom_registration(method)])

    feature_names = set([member[KEY] for member in origin_trial_constants + origin_trial_attributes + origin_trial_methods])

    # Construct the list of dictionaries. 'needs_instance' will be true if any
    # member for the feature has 'on_instance' defined as true.
    features = [{'name': name,
                 'constants': member_filter_by_name(origin_trial_constants, name),
                 'attributes': member_filter_by_name(origin_trial_attributes, name),
                 'methods': member_filter_by_name(origin_trial_methods, name)}
                for name in feature_names]
    for feature in features:
        members = feature['constants'] + feature['attributes'] + feature['methods']
        feature['needs_instance'] = any(member.get('on_instance', False) for member in members)
        # TODO(chasej): Need to handle method overloads? e.g.
        # (method['overloads']['secure_context_test_all'] if 'overloads' in method else method['secure_context_test'])
        feature['needs_secure_context'] = any(member.get('secure_context_test', False) for member in members)

    if features:
        includes.add('platform/bindings/ScriptState.h')
        includes.add('core/origin_trials/OriginTrials.h')
    return sorted(features)


def interface_context(interface, interfaces):
    """Creates a Jinja template context for an interface.

    Args:
        interface: An interface to create the context for
        interfaces: A dict which maps an interface name to the definition
            which can be referred if needed

    Returns:
        A Jinja template context for |interface|
    """

    includes.clear()
    includes.update(INTERFACE_CPP_INCLUDES)
    header_includes = set(INTERFACE_H_INCLUDES)

    if interface.is_partial:
        # A partial interface definition cannot specify that the interface
        # inherits from another interface. Inheritance must be specified on
        # the original interface definition.
        parent_interface = None
        is_event_target = False
        # partial interface needs the definition of its original interface.
        includes.add('bindings/core/v8/V8%s.h' % interface.name)
    else:
        parent_interface = interface.parent
        if parent_interface:
            header_includes.update(v8_types.includes_for_interface(parent_interface))
        is_event_target = inherits_interface(interface.name, 'EventTarget')

    extended_attributes = interface.extended_attributes

    is_array_buffer_or_view = interface.idl_type.is_array_buffer_or_view
    is_typed_array_type = interface.idl_type.is_typed_array
    if is_array_buffer_or_view:
        includes.update(('bindings/core/v8/V8ArrayBuffer.h',
                         'bindings/core/v8/V8SharedArrayBuffer.h'))
    if interface.name == 'ArrayBufferView':
        includes.update((
            'bindings/core/v8/V8Int8Array.h',
            'bindings/core/v8/V8Int16Array.h',
            'bindings/core/v8/V8Int32Array.h',
            'bindings/core/v8/V8Uint8Array.h',
            'bindings/core/v8/V8Uint8ClampedArray.h',
            'bindings/core/v8/V8Uint16Array.h',
            'bindings/core/v8/V8Uint32Array.h',
            'bindings/core/v8/V8Float32Array.h',
            'bindings/core/v8/V8Float64Array.h',
            'bindings/core/v8/V8DataView.h'))

    # [ActiveScriptWrappable]
    active_scriptwrappable = 'ActiveScriptWrappable' in extended_attributes

    # [CheckSecurity]
    is_check_security = 'CheckSecurity' in extended_attributes
    if is_check_security:
        includes.add('bindings/core/v8/BindingSecurity.h')
        includes.add('core/frame/LocalDOMWindow.h')

    # [DependentLifetime]
    is_dependent_lifetime = 'DependentLifetime' in extended_attributes

    # [PrimaryGlobal] and [Global]
    is_global = ('PrimaryGlobal' in extended_attributes or
                 'Global' in extended_attributes)

    # [ImmutablePrototype]
    # TODO(littledan): Is it possible to deduce this based on inheritance,
    # as in the WebIDL spec?
    is_immutable_prototype = is_global or 'ImmutablePrototype' in extended_attributes

    wrapper_class_id = ('kNodeClassId' if inherits_interface(interface.name, 'Node') else 'kObjectClassId')

    # [ActiveScriptWrappable] must be accompanied with [DependentLifetime].
    if active_scriptwrappable and not is_dependent_lifetime:
        raise Exception('[ActiveScriptWrappable] interface must also specify '
                        '[DependentLifetime]: %s' % interface.name)

    v8_class_name = v8_utilities.v8_class_name(interface)
    cpp_class_name = cpp_name(interface)
    cpp_class_name_or_partial = cpp_name_or_partial(interface)
    v8_class_name_or_partial = v8_utilities.v8_class_name_or_partial(interface)

    # TODO(peria): Generate the target list from 'Window' and 'HTMLDocument'.
    needs_runtime_enabled_installer = v8_class_name in [
        'V8Window', 'V8HTMLDocument', 'V8Document', 'V8Node', 'V8EventTarget']

    context = {
        'cpp_class': cpp_class_name,
        'cpp_class_or_partial': cpp_class_name_or_partial,
        'is_gc_type': True,
        # FIXME: Remove 'EventTarget' special handling, http://crbug.com/383699
        'has_access_check_callbacks': (is_check_security and
                                       interface.name != 'EventTarget'),
        'has_custom_legacy_call_as_function': has_extended_attribute_value(interface, 'Custom', 'LegacyCallAsFunction'),  # [Custom=LegacyCallAsFunction]
        'has_partial_interface': len(interface.partial_interfaces) > 0,
        'header_includes': header_includes,
        'interface_name': interface.name,
        'is_array_buffer_or_view': is_array_buffer_or_view,
        'is_check_security': is_check_security,
        'is_event_target': is_event_target,
        'is_exception': interface.is_exception,
        'is_global': is_global,
        'is_immutable_prototype': is_immutable_prototype,
        'is_node': inherits_interface(interface.name, 'Node'),
        'is_partial': interface.is_partial,
        'is_typed_array_type': is_typed_array_type,
        'lifetime': 'kDependent' if is_dependent_lifetime else 'kIndependent',
        'measure_as': v8_utilities.measure_as(interface, None),  # [MeasureAs]
        'needs_runtime_enabled_installer': needs_runtime_enabled_installer,
        'origin_trial_enabled_function': v8_utilities.origin_trial_enabled_function_name(interface),
        'parent_interface': parent_interface,
        'pass_cpp_type': cpp_name(interface) + '*',
        'active_scriptwrappable': active_scriptwrappable,
        'runtime_enabled_feature_name': runtime_enabled_feature_name(interface),  # [RuntimeEnabled]
        'v8_class': v8_class_name,
        'v8_class_or_partial': v8_class_name_or_partial,
        'wrapper_class_id': wrapper_class_id,
    }

    # Constructors
    constructors = [constructor_context(interface, constructor)
                    for constructor in interface.constructors
                    # FIXME: shouldn't put named constructors with constructors
                    # (currently needed for Perl compatibility)
                    # Handle named constructors separately
                    if constructor.name == 'Constructor']
    if len(constructors) > 1:
        context['constructor_overloads'] = overloads_context(interface, constructors)

    # [CustomConstructor]
    custom_constructors = [{  # Only needed for computing interface length
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    } for constructor in interface.custom_constructors]

    # [HTMLConstructor]
    has_html_constructor = 'HTMLConstructor' in extended_attributes
    # https://html.spec.whatwg.org/multipage/dom.html#html-element-constructors
    if has_html_constructor:
        if ('Constructor' in extended_attributes) or ('NoInterfaceObject' in extended_attributes):
            raise Exception('[Constructor] and [NoInterfaceObject] MUST NOT be'
                            ' specified with [HTMLConstructor]: '
                            '%s' % interface.name)
        includes.add('bindings/core/v8/V8HTMLConstructor.h')

    # [NamedConstructor]
    named_constructor = named_constructor_context(interface)

    if constructors or custom_constructors or named_constructor:
        if interface.is_partial:
            raise Exception('[Constructor] and [NamedConstructor] MUST NOT be'
                            ' specified on partial interface definitions: '
                            '%s' % interface.name)
        if named_constructor:
            includes.add('platform/bindings/V8PrivateProperty.h')

        includes.add('platform/bindings/V8ObjectConstructor.h')
        includes.add('core/frame/LocalDOMWindow.h')
    elif 'Measure' in extended_attributes or 'MeasureAs' in extended_attributes:
        if not interface.is_partial:
            raise Exception('[Measure] or [MeasureAs] specified for interface without a constructor: '
                            '%s' % interface.name)

    # [ConstructorCallWith=Document]
    if has_extended_attribute_value(interface, 'ConstructorCallWith', 'Document'):
        includes.add('core/dom/Document.h')

    # [Unscopable] attributes and methods
    unscopables = []
    for attribute in interface.attributes:
        if 'Unscopable' in attribute.extended_attributes:
            unscopables.append((attribute.name, runtime_enabled_feature_name(attribute)))
    for method in interface.operations:
        if 'Unscopable' in method.extended_attributes:
            unscopables.append((method.name, runtime_enabled_feature_name(method)))

    # [CEReactions]
    setter_or_deleters = (
        interface.indexed_property_setter,
        interface.indexed_property_deleter,
        interface.named_property_setter,
        interface.named_property_deleter,
    )
    has_ce_reactions = any(setter_or_deleter and 'CEReactions' in setter_or_deleter.extended_attributes
                           for setter_or_deleter in setter_or_deleters)
    if has_ce_reactions:
        includes.add('core/dom/custom/CEReactionsScope.h')

    context.update({
        'constructors': constructors,
        'has_custom_constructor': bool(custom_constructors),
        'has_html_constructor': has_html_constructor,
        'interface_length':
            interface_length(constructors + custom_constructors),
        'is_constructor_raises_exception': extended_attributes.get('RaisesException') == 'Constructor',  # [RaisesException=Constructor]
        'named_constructor': named_constructor,
        'unscopables': sorted(unscopables),
    })

    # Constants
    context.update({
        'constants': [constant_context(constant, interface) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in extended_attributes,
    })

    # Attributes
    attributes = attributes_context(interface, interfaces)

    context.update({
        'attributes': attributes,
        # Elements in attributes are broken in following members.
        'accessors': v8_attributes.filter_accessors(attributes),
        'data_attributes': v8_attributes.filter_data_attributes(attributes),
        'lazy_data_attributes': v8_attributes.filter_lazy_data_attributes(attributes),
        'runtime_enabled_attributes': v8_attributes.filter_runtime_enabled(attributes),
    })

    # Conditionally enabled attributes
    conditional_enabled_attributes = v8_attributes.filter_conditionally_enabled(attributes)
    has_conditional_attributes_on_prototype = any(  # pylint: disable=invalid-name
        attribute['on_prototype'] for attribute in conditional_enabled_attributes)
    context.update({
        'has_conditional_attributes_on_prototype':
            has_conditional_attributes_on_prototype,
        'conditionally_enabled_attributes': conditional_enabled_attributes,
    })

    # Methods
    context.update(methods_context(interface))
    methods = context['methods']
    context.update({
        'has_origin_safe_method_setter': any(method['is_cross_origin'] and not method['is_unforgeable']
            for method in methods),
        'conditionally_enabled_methods': v8_methods.filter_conditionally_enabled(methods, interface.is_partial),
    })

    # Window.idl in Blink has indexed properties, but the spec says Window
    # interface doesn't have indexed properties, instead the WindowProxy exotic
    # object has indexed properties.  Thus, Window interface must not support
    # iterators.
    has_array_iterator = (not interface.is_partial and
                          interface.has_indexed_elements and
                          interface.name != 'Window')
    context.update({
        'has_array_iterator': has_array_iterator,
        'iterable': interface.iterable,
    })

    # Conditionally enabled members
    prepare_prototype_and_interface_object_func = None  # pylint: disable=invalid-name
    if (unscopables or has_conditional_attributes_on_prototype or
            context['conditionally_enabled_methods']):
        prepare_prototype_and_interface_object_func = '%s::preparePrototypeAndInterfaceObject' % v8_class_name_or_partial  # pylint: disable=invalid-name

    context.update({
        'prepare_prototype_and_interface_object_func': prepare_prototype_and_interface_object_func,
    })

    context.update({
        'legacy_caller': legacy_caller(interface.legacy_caller, interface),
        'indexed_property_getter': property_getter(interface.indexed_property_getter, ['index']),
        'indexed_property_setter': property_setter(interface.indexed_property_setter, interface),
        'indexed_property_deleter': property_deleter(interface.indexed_property_deleter),
        'is_override_builtins': 'OverrideBuiltins' in extended_attributes,
        'named_property_getter': property_getter(interface.named_property_getter, ['name']),
        'named_property_setter': property_setter(interface.named_property_setter, interface),
        'named_property_deleter': property_deleter(interface.named_property_deleter),
    })
    context.update({
        'has_named_properties_object': is_global and context['named_property_getter'],
    })

    # Origin Trials
    context.update({
        'origin_trial_features': origin_trial_features(interface, context['constants'], context['attributes'], context['methods']),
    })

    # Cross-origin interceptors
    has_cross_origin_named_getter = False
    has_cross_origin_named_setter = False
    has_cross_origin_indexed_getter = False

    for attribute in attributes:
        if attribute['has_cross_origin_getter']:
            has_cross_origin_named_getter = True
        if attribute['has_cross_origin_setter']:
            has_cross_origin_named_setter = True

    # Methods are exposed as getter attributes on the interface: e.g.
    # window.location gets the location attribute on the Window interface. For
    # the cross-origin case, this attribute getter is guaranteed to only return
    # a Function object, which the actual call is dispatched against.
    for method in methods:
        if method['is_cross_origin']:
            has_cross_origin_named_getter = True

    has_cross_origin_named_enumerator = has_cross_origin_named_getter or has_cross_origin_named_setter  # pylint: disable=invalid-name

    if context['named_property_getter'] and context['named_property_getter']['is_cross_origin']:
        has_cross_origin_named_getter = True

    if context['indexed_property_getter'] and context['indexed_property_getter']['is_cross_origin']:
        has_cross_origin_indexed_getter = True

    context.update({
        'has_cross_origin_named_getter': has_cross_origin_named_getter,
        'has_cross_origin_named_setter': has_cross_origin_named_setter,
        'has_cross_origin_named_enumerator': has_cross_origin_named_enumerator,
        'has_cross_origin_indexed_getter': has_cross_origin_indexed_getter,
    })

    return context


def attributes_context(interface, interfaces):
    """Creates a list of Jinja template contexts for attributes of an interface.

    Args:
        interface: An interface to create contexts for
        interfaces: A dict which maps an interface name to the definition
            which can be referred if needed

    Returns:
        A list of attribute contexts
    """

    attributes = [v8_attributes.attribute_context(interface, attribute, interfaces)
                  for attribute in interface.attributes]

    has_conditional_attributes = any(attribute['exposed_test'] for attribute in attributes)
    if has_conditional_attributes and interface.is_partial:
        raise Exception(
            'Conditional attributes between partial interfaces in modules '
            'and the original interfaces(%s) in core are not allowed.'
            % interface.name)

    # See also comment in methods_context.
    if not interface.is_partial and (interface.maplike or interface.setlike):
        if any(attribute['name'] == 'size' for attribute in attributes):
            raise ValueError(
                'An interface cannot define an attribute called "size"; it is '
                'implied by maplike/setlike in the IDL.')
        size_attribute = IdlAttribute()
        size_attribute.name = 'size'
        size_attribute.idl_type = IdlType('unsigned long')
        size_attribute.is_read_only = True
        size_attribute.extended_attributes['NotEnumerable'] = None
        attributes.append(v8_attributes.attribute_context(
            interface, size_attribute, interfaces))

    return attributes


def methods_context(interface):
    """Creates a list of Jinja template contexts for methods of an interface.

    Args:
        interface: An interface to create contexts for

    Returns:
        A dictionary with 3 keys:
        'iterator_method': An iterator context if available or None.
        'iterator_method_alias': A string that can also be used to refer to the
                                 @@iterator symbol or None.
        'methods': A list of method contexts.
    """

    methods = []

    if interface.original_interface:
        methods.extend([v8_methods.method_context(interface, operation, is_visible=False)
                        for operation in interface.original_interface.operations
                        if operation.name])
    methods.extend([v8_methods.method_context(interface, method)
                    for method in interface.operations
                    if method.name])  # Skip anonymous special operations (methods)
    if interface.partial_interfaces:
        assert len(interface.partial_interfaces) == len(set(interface.partial_interfaces))
        for partial_interface in interface.partial_interfaces:
            methods.extend([v8_methods.method_context(interface, operation, is_visible=False)
                            for operation in partial_interface.operations
                            if operation.name])
    compute_method_overloads_context(interface, methods)

    def generated_method(return_type, name, arguments=None, extended_attributes=None, implemented_as=None):
        operation = IdlOperation()
        operation.idl_type = return_type
        operation.name = name
        if arguments:
            operation.arguments = arguments
        if extended_attributes:
            operation.extended_attributes.update(extended_attributes)
        if implemented_as is None:
            implemented_as = name + 'ForBinding'
        operation.extended_attributes['ImplementedAs'] = implemented_as
        return v8_methods.method_context(interface, operation)

    def generated_argument(idl_type, name, is_optional=False, extended_attributes=None):
        argument = IdlArgument()
        argument.idl_type = idl_type
        argument.name = name
        argument.is_optional = is_optional
        if extended_attributes:
            argument.extended_attributes.update(extended_attributes)
        return argument

    # iterable<>, maplike<> and setlike<>
    iterator_method = None

    # Depending on the declaration, @@iterator may be a synonym for e.g.
    # 'entries' or 'values'.
    iterator_method_alias = None

    # FIXME: support Iterable in partial interfaces. However, we don't
    # need to support iterator overloads between interface and
    # partial interface definitions.
    # http://heycam.github.io/webidl/#idl-overloading
    if (not interface.is_partial and (
            interface.iterable or interface.maplike or interface.setlike or
            interface.has_indexed_elements)):

        used_extended_attributes = {}

        if interface.iterable:
            used_extended_attributes.update(interface.iterable.extended_attributes)
        elif interface.maplike:
            used_extended_attributes.update(interface.maplike.extended_attributes)
        elif interface.setlike:
            used_extended_attributes.update(interface.setlike.extended_attributes)

        if 'RaisesException' in used_extended_attributes:
            raise ValueError('[RaisesException] is implied for iterable<>/maplike<>/setlike<>')
        if 'CallWith' in used_extended_attributes:
            raise ValueError('[CallWith=ScriptState] is implied for iterable<>/maplike<>/setlike<>')

        used_extended_attributes.update({
            'RaisesException': None,
            'CallWith': 'ScriptState',
        })

        forEach_extended_attributes = used_extended_attributes.copy()
        forEach_extended_attributes.update({
            'CallWith': ['ScriptState', 'ThisValue'],
        })

        def generated_iterator_method(name, implemented_as=None):
            return generated_method(
                return_type=IdlType('Iterator'),
                name=name,
                extended_attributes=used_extended_attributes,
                implemented_as=implemented_as)

        if not interface.has_indexed_elements:
            iterator_method = generated_iterator_method('iterator', implemented_as='GetIterator')

        if interface.iterable or interface.maplike or interface.setlike:
            non_overridable_methods = []
            overridable_methods = []

            is_value_iterator = interface.iterable and interface.iterable.key_type is None

            # For value iterators, the |entries|, |forEach|, |keys| and |values| are originally set
            # to corresponding properties in %ArrayPrototype%.
            # For pair iterators and maplike declarations, |entries| is an alias for @@iterator
            # itself. For setlike declarations, |values| is an alias for @@iterator.
            if not is_value_iterator:
                if not interface.setlike:
                    iterator_method_alias = 'entries'
                    entries_or_values_method = generated_iterator_method('values')
                else:
                    iterator_method_alias = 'values'
                    entries_or_values_method = generated_iterator_method('entries')

                non_overridable_methods.extend([
                    generated_iterator_method('keys'),
                    entries_or_values_method,

                    # void forEach(Function callback, [Default=Undefined] optional any thisArg)
                    generated_method(IdlType('void'), 'forEach',
                                     arguments=[generated_argument(IdlType('Function'), 'callback'),
                                                generated_argument(IdlType('any'), 'thisArg',
                                                                   is_optional=True,
                                                                   extended_attributes={'Default': 'Undefined'})],
                                     extended_attributes=forEach_extended_attributes),
                ])

            if interface.maplike:
                key_argument = generated_argument(interface.maplike.key_type, 'key')
                value_argument = generated_argument(interface.maplike.value_type, 'value')

                non_overridable_methods.extend([
                    generated_method(IdlType('boolean'), 'has',
                                     arguments=[key_argument],
                                     extended_attributes=used_extended_attributes),
                    generated_method(IdlType('any'), 'get',
                                     arguments=[key_argument],
                                     extended_attributes=used_extended_attributes),
                ])

                if not interface.maplike.is_read_only:
                    overridable_methods.extend([
                        generated_method(IdlType('void'), 'clear',
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('boolean'), 'delete',
                                         arguments=[key_argument],
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType(interface.name), 'set',
                                         arguments=[key_argument, value_argument],
                                         extended_attributes=used_extended_attributes),
                    ])

            if interface.setlike:
                value_argument = generated_argument(interface.setlike.value_type, 'value')

                non_overridable_methods.extend([
                    generated_method(IdlType('boolean'), 'has',
                                     arguments=[value_argument],
                                     extended_attributes=used_extended_attributes),
                ])

                if not interface.setlike.is_read_only:
                    overridable_methods.extend([
                        generated_method(IdlType(interface.name), 'add',
                                         arguments=[value_argument],
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('void'), 'clear',
                                         extended_attributes=used_extended_attributes),
                        generated_method(IdlType('boolean'), 'delete',
                                         arguments=[value_argument],
                                         extended_attributes=used_extended_attributes),
                    ])

            methods_by_name = {}
            for method in methods:
                methods_by_name.setdefault(method['name'], []).append(method)

            for non_overridable_method in non_overridable_methods:
                if non_overridable_method['name'] in methods_by_name:
                    raise ValueError(
                        'An interface cannot define an operation called "%s()", it '
                        'comes from the iterable, maplike or setlike declaration '
                        'in the IDL.' % non_overridable_method['name'])
                methods.append(non_overridable_method)

            for overridable_method in overridable_methods:
                if overridable_method['name'] in methods_by_name:
                    # FIXME: Check that the existing method is compatible.
                    continue
                methods.append(overridable_method)

        # FIXME: maplike<> and setlike<> should also imply the presence of a
        # 'size' attribute.

    # Serializer
    if interface.serializer:
        serializer = interface.serializer
        serializer_ext_attrs = serializer.extended_attributes.copy()
        if serializer.operation:
            return_type = serializer.operation.idl_type
            implemented_as = serializer.operation.name
        else:
            return_type = IdlType('any')
            implemented_as = None
            if 'CallWith' not in serializer_ext_attrs:
                serializer_ext_attrs['CallWith'] = 'ScriptState'
        methods.append(generated_method(
            return_type=return_type,
            name='toJSON',
            extended_attributes=serializer_ext_attrs,
            implemented_as=implemented_as))

    # Stringifier
    if interface.stringifier:
        stringifier = interface.stringifier
        stringifier_ext_attrs = stringifier.extended_attributes.copy()
        if stringifier.attribute:
            implemented_as = stringifier.attribute.name
        elif stringifier.operation:
            implemented_as = stringifier.operation.name
        else:
            implemented_as = 'toString'
        methods.append(generated_method(
            return_type=IdlType('DOMString'),
            name='toString',
            extended_attributes=stringifier_ext_attrs,
            implemented_as=implemented_as))

    for method in methods:
        # The value of the Function object’s “length” property is a Number
        # determined as follows:
        # 1. Let S be the effective overload set for regular operations (if the
        # operation is a regular operation) or for static operations (if the
        # operation is a static operation) with identifier id on interface I and
        # with argument count 0.
        # 2. Return the length of the shortest argument list of the entries in S.
        # FIXME: This calculation doesn't take into account whether runtime
        # enabled overloads are actually enabled, so length may be incorrect.
        # E.g., [RuntimeEnabled=Foo] void f(); void f(long x);
        # should have length 1 if Foo is not enabled, but length 0 if it is.
        method['length'] = (method['overloads']['length'] if 'overloads' in method else
                            method['number_of_required_arguments'])

    return {
        'iterator_method': iterator_method,
        'iterator_method_alias': iterator_method_alias,
        'methods': methods,
    }


def reflected_name(constant_name):
    """Returns the name to use for the matching constant name in blink code.

    Given an all-uppercase 'CONSTANT_NAME', returns a camel-case
    'kConstantName'.
    """
    # Check for SHOUTY_CASE constants
    if constant_name.upper() != constant_name:
        return constant_name
    return 'k' + ''.join(part.title() for part in constant_name.split('_'))


# [DeprecateAs], [OriginTrialEnabled], [Reflect], [RuntimeEnabled]
def constant_context(constant, interface):
    extended_attributes = constant.extended_attributes

    return {
        'cpp_class': extended_attributes.get('PartialInterfaceImplementedAs'),
        'deprecate_as': v8_utilities.deprecate_as(constant),  # [DeprecateAs]
        'idl_type': constant.idl_type.name,
        'measure_as': v8_utilities.measure_as(constant, interface),  # [MeasureAs]
        'name': constant.name,
        'origin_trial_enabled_function': v8_utilities.origin_trial_enabled_function_name(constant),  # [OriginTrialEnabled]
        'origin_trial_feature_name': v8_utilities.origin_trial_feature_name(constant),  # [OriginTrialEnabled]
        # FIXME: use 'reflected_name' as correct 'name'
        'reflected_name': extended_attributes.get('Reflect', reflected_name(constant.name)),
        'runtime_enabled_feature_name': runtime_enabled_feature_name(constant),  # [RuntimeEnabled]
        'value': constant.value,
    }


################################################################################
# Overloads
################################################################################

def compute_method_overloads_context(interface, methods):
    # Regular methods
    compute_method_overloads_context_by_type(
        interface, [method for method in methods if not method['is_static']])
    # Static methods
    compute_method_overloads_context_by_type(
        interface, [method for method in methods if method['is_static']])


def compute_method_overloads_context_by_type(interface, methods):
    """Computes |method.overload*| template values.

    Called separately for static and non-static (regular) methods,
    as these are overloaded separately.
    Modifies |method| in place for |method| in |methods|.
    Doesn't change the |methods| list itself (only the values, i.e. individual
    methods), so ok to treat these separately.
    """
    # Add overload information only to overloaded methods, so template code can
    # easily verify if a function is overloaded
    for name, overloads in method_overloads_by_name(methods):
        # Resolution function is generated after last overloaded function;
        # package necessary information into |method.overloads| for that method.
        overloads[-1]['overloads'] = overloads_context(interface, overloads)
        overloads[-1]['overloads']['name'] = name




def overloads_context(interface, overloads):
    """Returns |overloads| template values for a single name.

    Sets |method.overload_index| in place for |method| in |overloads|
    and returns dict of overall overload template values.
    """
    assert len(overloads) > 1  # only apply to overloaded names
    for index, method in enumerate(overloads, 1):
        method['overload_index'] = index

    # [OriginTrialEnabled]
    # TODO(iclelland): Allow origin trials on method overloads
    # (crbug.com/621641)
    if any(method.get('origin_trial_feature_name') for method in overloads):
        raise Exception('[OriginTrialEnabled] cannot be specified on '
                        'overloaded methods: %s.%s' % (interface.name, overloads[0]['name']))

    effective_overloads_by_length = effective_overload_set_by_length(overloads)
    lengths = [length for length, _ in effective_overloads_by_length]
    name = overloads[0].get('name', '<constructor>')

    runtime_determined_lengths = None
    function_length = lengths[0]
    runtime_determined_maxargs = None
    maxarg = lengths[-1]

    # The special case handling below is not needed if all overloads are
    # runtime enabled by the same feature.
    if not common_value(overloads, 'runtime_enabled_feature_name'):
        # Check if all overloads with the shortest acceptable arguments list are
        # runtime enabled, in which case we need to have a runtime determined
        # Function.length.
        shortest_overloads = effective_overloads_by_length[0][1]
        if (all(method.get('runtime_enabled_feature_name')
                for method, _, _ in shortest_overloads)):
            # Generate a list of (length, runtime_enabled_feature_names) tuples.
            runtime_determined_lengths = []
            for length, effective_overloads in effective_overloads_by_length:
                runtime_enabled_feature_names = set(
                    method['runtime_enabled_feature_name']
                    for method, _, _ in effective_overloads
                    if method.get('runtime_enabled_feature_name'))
                if not runtime_enabled_feature_names:
                    # This "length" is unconditionally enabled, so stop here.
                    runtime_determined_lengths.append((length, [None]))
                    break
                runtime_determined_lengths.append(
                    (length, sorted(runtime_enabled_feature_names)))
            function_length = ('%sV8Internal::%sMethodLength()'
                               % (cpp_name_or_partial(interface), name))

        # Check if all overloads with the longest required arguments list are
        # runtime enabled, in which case we need to have a runtime determined
        # maximum distinguishing argument index.
        longest_overloads = effective_overloads_by_length[-1][1]
        if (not common_value(overloads, 'runtime_enabled_feature_name') and
                all(method.get('runtime_enabled_feature_name')
                    for method, _, _ in longest_overloads)):
            # Generate a list of (length, runtime_enabled_feature_name) tuples.
            runtime_determined_maxargs = []
            for length, effective_overloads in reversed(effective_overloads_by_length):
                runtime_enabled_feature_names = set(
                    method['runtime_enabled_feature_name']
                    for method, _, _ in effective_overloads
                    if method.get('runtime_enabled_feature_name'))
                if not runtime_enabled_feature_names:
                    # This "length" is unconditionally enabled, so stop here.
                    runtime_determined_maxargs.append((length, [None]))
                    break
                runtime_determined_maxargs.append(
                    (length, sorted(runtime_enabled_feature_names)))
            maxarg = ('%sV8Internal::%sMethodMaxArg()'
                      % (cpp_name_or_partial(interface), name))

    # Check and fail if overloads disagree about whether the return type
    # is a Promise or not.
    promise_overload_count = sum(1 for method in overloads if method.get('returns_promise'))
    if promise_overload_count not in (0, len(overloads)):
        raise ValueError('Overloads of %s have conflicting Promise/non-Promise types'
                         % (name))

    has_overload_visible = False
    has_overload_not_visible = False
    for overload in overloads:
        if overload.get('visible', True):
            # If there exists an overload which is visible, need to generate
            # overload_resolution, i.e. overlods_visible should be True.
            has_overload_visible = True
        else:
            has_overload_not_visible = True

    # If some overloads are not visible and others are visible,
    # the method is overloaded between core and modules.
    has_partial_overloads = has_overload_visible and has_overload_not_visible

    return {
        'deprecate_all_as': common_value(overloads, 'deprecate_as'),  # [DeprecateAs]
        'exposed_test_all': common_value(overloads, 'exposed_test'),  # [Exposed]
        'length': function_length,
        'length_tests_methods': length_tests_methods(effective_overloads_by_length),
        # 1. Let maxarg be the length of the longest type list of the
        # entries in S.
        'maxarg': maxarg,
        'measure_all_as': common_value(overloads, 'measure_as'),  # [MeasureAs]
        'returns_promise_all': promise_overload_count > 0,
        'runtime_determined_lengths': runtime_determined_lengths,
        'runtime_determined_maxargs': runtime_determined_maxargs,
        'runtime_enabled_all': common_value(overloads, 'runtime_enabled_feature_name'),  # [RuntimeEnabled]
        'secure_context_test_all': common_value(overloads, 'secure_context_test'),  # [SecureContext]
        'valid_arities': (lengths
                          # Only need to report valid arities if there is a gap in the
                          # sequence of possible lengths, otherwise invalid length means
                          # "not enough arguments".
                          if lengths[-1] - lengths[0] != len(lengths) - 1 else None),
        'visible': has_overload_visible,
        'has_partial_overloads': has_partial_overloads,
    }



def distinguishing_argument_index(entries):
    """Returns the distinguishing argument index for a sequence of entries.

    Entries are elements of the effective overload set with the same number
    of arguments (formally, same type list length), each a 3-tuple of the form
    (callable, type list, optionality list).

    Spec: http://heycam.github.io/webidl/#dfn-distinguishing-argument-index

    If there is more than one entry in an effective overload set that has a
    given type list length, then for those entries there must be an index i
    such that for each pair of entries the types at index i are
    distinguishable.
    The lowest such index is termed the distinguishing argument index for the
    entries of the effective overload set with the given type list length.
    """
    # Only applicable “If there is more than one entry”
    assert len(entries) > 1

    def typename_without_nullable(idl_type):
        if idl_type.is_nullable:
            return idl_type.inner_type.name
        return idl_type.name

    type_lists = [tuple(typename_without_nullable(idl_type)
                        for idl_type in entry[1])
                  for entry in entries]
    type_list_length = len(type_lists[0])
    # Only applicable for entries that “[have] a given type list length”
    assert all(len(type_list) == type_list_length for type_list in type_lists)
    name = entries[0][0].get('name', 'Constructor')  # for error reporting

    # The spec defines the distinguishing argument index by conditions it must
    # satisfy, but does not give an algorithm.
    #
    # We compute the distinguishing argument index by first computing the
    # minimum index where not all types are the same, and then checking that
    # all types in this position are distinguishable (and the optionality lists
    # up to this point are identical), since "minimum index where not all types
    # are the same" is a *necessary* condition, and more direct to check than
    # distinguishability.
    types_by_index = (set(types) for types in zip(*type_lists))
    try:
        # “In addition, for each index j, where j is less than the
        #  distinguishing argument index for a given type list length, the types
        #  at index j in all of the entries’ type lists must be the same”
        index = next(i for i, types in enumerate(types_by_index)
                     if len(types) > 1)
    except StopIteration:
        raise ValueError('No distinguishing index found for %s, length %s:\n'
                         'All entries have the same type list:\n'
                         '%s' % (name, type_list_length, type_lists[0]))
    # Check optionality
    # “and the booleans in the corresponding list indicating argument
    #  optionality must be the same.”
    # FIXME: spec typo: optionality value is no longer a boolean
    # https://www.w3.org/Bugs/Public/show_bug.cgi?id=25628
    initial_optionality_lists = set(entry[2][:index] for entry in entries)
    if len(initial_optionality_lists) > 1:
        raise ValueError(
            'Invalid optionality lists for %s, length %s:\n'
            'Optionality lists differ below distinguishing argument index %s:\n'
            '%s'
            % (name, type_list_length, index, set(initial_optionality_lists)))

    # Check distinguishability
    # http://heycam.github.io/webidl/#dfn-distinguishable
    # Use names to check for distinct types, since objects are distinct
    # FIXME: check distinguishability more precisely, for validation
    distinguishing_argument_type_names = [type_list[index]
                                          for type_list in type_lists]
    if (len(set(distinguishing_argument_type_names)) !=
            len(distinguishing_argument_type_names)):
        raise ValueError('Types in distinguishing argument are not distinct:\n'
                         '%s' % distinguishing_argument_type_names)

    return index


def length_tests_methods(effective_overloads_by_length):
    """Returns sorted list of resolution tests and associated methods, by length.

    This builds the main data structure for the overload resolution loop.
    For a given argument length, bindings test argument at distinguishing
    argument index, in order given by spec: if it is compatible with
    (optionality or) type required by an overloaded method, resolve to that
    method.

    Returns:
        [(length, [(test, method)])]
    """
    return [(length, list(resolution_tests_methods(effective_overloads)))
            for length, effective_overloads in effective_overloads_by_length]


def resolution_tests_methods(effective_overloads):
    """Yields resolution test and associated method, in resolution order, for effective overloads of a given length.

    This is the heart of the resolution algorithm.
    http://heycam.github.io/webidl/#dfn-overload-resolution-algorithm

    Note that a given method can be listed multiple times, with different tests!
    This is to handle implicit type conversion.

    Returns:
        [(test, method)]
    """
    methods = [effective_overload[0]
               for effective_overload in effective_overloads]
    if len(methods) == 1:
        # If only one method with a given length, no test needed
        yield 'true', methods[0]
        return

    # 6. If there is more than one entry in S, then set d to be the
    # distinguishing argument index for the entries of S.
    index = distinguishing_argument_index(effective_overloads)
    # (7-9 are for handling |undefined| values for optional arguments before
    # the distinguishing argument (as “missing”), so you can specify only some
    # optional arguments. We don’t support this, so we skip these steps.)
    # 10. If i = d, then:
    # (d is the distinguishing argument index)
    # 1. Let V be argi.
    #     Note: This is the argument that will be used to resolve which
    #           overload is selected.
    cpp_value = 'info[%s]' % index

    # Extract argument and IDL type to simplify accessing these in each loop.
    arguments = [method['arguments'][index] for method in methods]
    arguments_methods = zip(arguments, methods)
    idl_types = [argument['idl_type_object'] for argument in arguments]
    idl_types_methods = zip(idl_types, methods)

    # We can’t do a single loop through all methods or simply sort them, because
    # a method may be listed in multiple steps of the resolution algorithm, and
    # which test to apply differs depending on the step.
    #
    # Instead, we need to go through all methods at each step, either finding
    # first match (if only one test is allowed) or filtering to matches (if
    # multiple tests are allowed), and generating an appropriate tests.

    # 2. If V is undefined, and there is an entry in S whose list of
    # optionality values has “optional” at index i, then remove from S all
    # other entries.
    try:
        method = next(method for argument, method in arguments_methods
                      if argument['is_optional'])
        test = '%s->IsUndefined()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 3. Otherwise: if V is null or undefined, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • a nullable type
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_nullable)
        test = 'IsUndefinedOrNull(%s)' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 4. Otherwise: if V is a platform object – but not a platform array
    # object – and there is an entry in S that has one of the following
    # types at position i of its type list,
    # • an interface type that V implements
    # (Unlike most of these tests, this can return multiple methods, since we
    #  test if it implements an interface. Thus we need a for loop, not a next.)
    # (We distinguish wrapper types from built-in interface types.)
    for idl_type, method in ((idl_type, method)
                             for idl_type, method in idl_types_methods
                             if idl_type.is_wrapper_type):
        if idl_type.is_array_buffer_or_view:
            test = '{cpp_value}->Is{idl_type}()'.format(idl_type=idl_type.base_type, cpp_value=cpp_value)
        else:
            test = 'V8{idl_type}::hasInstance({cpp_value}, info.GetIsolate())'.format(idl_type=idl_type.base_type, cpp_value=cpp_value)
        yield test, method

    # 13. Otherwise: if IsCallable(V) is true, and there is an entry in S that
    # has one of the following types at position i of its type list,
    # • a callback function type
    # ...
    #
    # FIXME:
    # We test for functions rather than callability, which isn't strictly the
    # same thing.
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_custom_callback_function)
        test = '%s->IsFunction()' % cpp_value
        yield test, method
    except StopIteration:
        pass

    # 14. Otherwise: if V is any kind of object except for a native Date object,
    # a native RegExp object, and there is an entry in S that has one of the
    # following types at position i of its type list,
    # • a sequence type
    # ...
    #
    # 15. Otherwise: if V is any kind of object except for a native Date object,
    # a native RegExp object, and there is an entry in S that has one of the
    # following types at position i of its type list,
    # • an array type
    # ...
    # • a dictionary
    #
    # FIXME:
    # We don't strictly follow the algorithm here. The algorithm says "remove
    # all other entries" if there is "one entry" matching, but we yield all
    # entries to support following constructors:
    # [constructor(sequence<DOMString> arg), constructor(Dictionary arg)]
    # interface I { ... }
    # (Need to check array types before objects because an array is an object)
    for idl_type, method in idl_types_methods:
        if idl_type.native_array_element_type:
            # (We test for Array instead of generic Object to type-check.)
            # FIXME: test for Object during resolution, then have type check for
            # Array in overloaded method: http://crbug.com/262383
            yield '%s->IsArray()' % cpp_value, method
    for idl_type, method in idl_types_methods:
        if idl_type.is_dictionary or idl_type.name == 'Dictionary' or \
           idl_type.is_callback_interface or idl_type.is_record_type:
            # FIXME: should be '{1}->IsObject() && !{1}->IsRegExp()'.format(cpp_value)
            # FIXME: the IsRegExp checks can be skipped if we've
            # already generated tests for them.
            yield '%s->IsObject()' % cpp_value, method

    # (Check for exact type matches before performing automatic type conversion;
    # only needed if distinguishing between primitive types.)
    if len([idl_type.is_primitive_type for idl_type in idl_types]) > 1:
        # (Only needed if match in step 11, otherwise redundant.)
        if any(idl_type.is_string_type or idl_type.is_enum
               for idl_type in idl_types):
            # 10. Otherwise: if V is a Number value, and there is an entry in S
            # that has one of the following types at position i of its type
            # list,
            # • a numeric type
            try:
                method = next(method for idl_type, method in idl_types_methods
                              if idl_type.is_numeric_type)
                test = '%s->IsNumber()' % cpp_value
                yield test, method
            except StopIteration:
                pass

    # (Perform automatic type conversion, in order. If any of these match,
    # that’s the end, and no other tests are needed.) To keep this code simple,
    # we rely on the C++ compiler's dead code elimination to deal with the
    # redundancy if both cases below trigger.

    # 11. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • DOMString
    # • ByteString
    # • USVString
    # • an enumeration type
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_string_type or idl_type.is_enum)
        yield 'true', method
    except StopIteration:
        pass

    # 12. Otherwise: if there is an entry in S that has one of the following
    # types at position i of its type list,
    # • a numeric type
    try:
        method = next(method for idl_type, method in idl_types_methods
                      if idl_type.is_numeric_type)
        yield 'true', method
    except StopIteration:
        pass


################################################################################
# Utility functions
################################################################################

def common(dicts, f):
    """Returns common result of f across an iterable of dicts, or None.

    Call f for each dict and return its result if the same across all dicts.
    """
    values = (f(d) for d in dicts)
    first_value = next(values)
    if all(value == first_value for value in values):
        return first_value
    return None


def common_key(dicts, key):
    """Returns common presence of a key across an iterable of dicts, or None.

    True if all dicts have the key, False if none of the dicts have the key,
    and None if some but not all dicts have the key.
    """
    return common(dicts, lambda d: key in d)


def common_value(dicts, key):
    """Returns common value of a key across an iterable of dicts, or None.

    Auxiliary function for overloads, so can consolidate an extended attribute
    that appears with the same value on all items in an overload set.
    """
    return common(dicts, lambda d: d.get(key))


################################################################################
# Constructors
################################################################################

# [Constructor]
def constructor_context(interface, constructor):
    # [RaisesException=Constructor]
    is_constructor_raises_exception = \
        interface.extended_attributes.get('RaisesException') == 'Constructor'

    argument_contexts = [
        v8_methods.argument_context(interface, constructor, argument, index)
        for index, argument in enumerate(constructor.arguments)]

    return {
        'arguments': argument_contexts,
        'cpp_type': cpp_name(interface) + '*',
        'cpp_value': v8_methods.cpp_value(
            interface, constructor, len(constructor.arguments)),
        'has_exception_state':
            is_constructor_raises_exception or
            any(argument for argument in constructor.arguments
                if argument.idl_type.name == 'SerializedScriptValue' or
                argument.idl_type.v8_conversion_needs_exception_state),
        'has_optional_argument_without_default_value':
            any(True for argument_context in argument_contexts
                if argument_context['is_optional_without_default_value']),
        'is_call_with_document':
            # [ConstructorCallWith=Document]
            has_extended_attribute_value(interface,
                                         'ConstructorCallWith', 'Document'),
        'is_call_with_execution_context':
            # [ConstructorCallWith=ExecutionContext]
            has_extended_attribute_value(interface,
                                         'ConstructorCallWith', 'ExecutionContext'),
        'is_call_with_script_state':
            # [ConstructorCallWith=ScriptState]
            has_extended_attribute_value(
                interface, 'ConstructorCallWith', 'ScriptState'),
        'is_constructor': True,
        'is_named_constructor': False,
        'is_raises_exception': is_constructor_raises_exception,
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    }


# [NamedConstructor]
def named_constructor_context(interface):
    extended_attributes = interface.extended_attributes
    if 'NamedConstructor' not in extended_attributes:
        return None
    # FIXME: parser should return named constructor separately;
    # included in constructors (and only name stored in extended attribute)
    # for Perl compatibility
    idl_constructor = interface.constructors[-1]
    assert idl_constructor.name == 'NamedConstructor'
    context = constructor_context(interface, idl_constructor)
    context.update({
        'name': extended_attributes['NamedConstructor'],
        'is_named_constructor': True,
    })
    return context


def number_of_required_arguments(constructor):
    return len([argument for argument in constructor.arguments
                if not argument.is_optional])


def interface_length(constructors):
    # Docs: http://heycam.github.io/webidl/#es-interface-call
    if not constructors:
        return 0
    return min(constructor['number_of_required_arguments']
               for constructor in constructors)


################################################################################
# Special operations (methods)
# http://heycam.github.io/webidl/#idl-special-operations
################################################################################

def legacy_caller(caller, interface):
    if not caller:
        return None

    return v8_methods.method_context(interface, caller)

def property_getter(getter, cpp_arguments):
    if not getter:
        return None

    def is_null_expression(idl_type):
        if idl_type.use_output_parameter_for_result:
            return 'result.isNull()'
        if idl_type.is_string_type:
            return 'result.IsNull()'
        if idl_type.is_interface_type:
            return '!result'
        if idl_type.base_type in ('any', 'object'):
            return 'result.IsEmpty()'
        return ''

    extended_attributes = getter.extended_attributes
    idl_type = getter.idl_type
    idl_type.add_includes_for_type(extended_attributes)
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(getter, 'CallWith', 'ScriptState')
    is_raises_exception = 'RaisesException' in extended_attributes
    use_output_parameter_for_result = idl_type.use_output_parameter_for_result

    # FIXME: make more generic, so can use v8_methods.cpp_value
    cpp_method_name = 'impl->%s' % cpp_name(getter)

    if is_call_with_script_state:
        cpp_arguments.insert(0, 'scriptState')
    if is_raises_exception:
        cpp_arguments.append('exceptionState')
    if use_output_parameter_for_result:
        cpp_arguments.append('result')

    cpp_value = '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))

    return {
        'cpp_type': idl_type.cpp_type,
        'cpp_value': cpp_value,
        'is_call_with_script_state': is_call_with_script_state,
        'is_cross_origin': 'CrossOrigin' in extended_attributes,
        'is_custom':
            'Custom' in extended_attributes and
            (not extended_attributes['Custom'] or
             has_extended_attribute_value(getter, 'Custom', 'PropertyGetter')),
        'is_custom_property_enumerator': has_extended_attribute_value(
            getter, 'Custom', 'PropertyEnumerator'),
        'is_custom_property_query': has_extended_attribute_value(
            getter, 'Custom', 'PropertyQuery'),
        'is_enumerable': 'NotEnumerable' not in extended_attributes,
        'is_null_expression': is_null_expression(idl_type),
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(getter),
        'use_output_parameter_for_result': use_output_parameter_for_result,
        'v8_set_return_value': idl_type.v8_set_return_value('result', extended_attributes=extended_attributes, script_wrappable='impl'),
    }


def property_setter(setter, interface):
    if not setter:
        return None

    extended_attributes = setter.extended_attributes
    idl_type = setter.arguments[1].idl_type
    idl_type.add_includes_for_type(extended_attributes)
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(setter, 'CallWith', 'ScriptState')
    is_raises_exception = 'RaisesException' in extended_attributes
    is_ce_reactions = 'CEReactions' in extended_attributes

    # [LegacyInterfaceTypeChecking]
    has_type_checking_interface = (
        not is_legacy_interface_type_checking(interface, setter) and
        idl_type.is_wrapper_type)

    return {
        'has_exception_state': (is_raises_exception or
                                idl_type.v8_conversion_needs_exception_state),
        'has_type_checking_interface': has_type_checking_interface,
        'idl_type': idl_type.base_type,
        'is_call_with_script_state': is_call_with_script_state,
        'is_ce_reactions': is_ce_reactions,
        'is_custom': 'Custom' in extended_attributes,
        'is_nullable': idl_type.is_nullable,
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(setter),
        'v8_value_to_local_cpp_value': idl_type.v8_value_to_local_cpp_value(
            extended_attributes, 'v8Value', 'propertyValue'),
    }


def property_deleter(deleter):
    if not deleter:
        return None

    extended_attributes = deleter.extended_attributes
    is_call_with_script_state = v8_utilities.has_extended_attribute_value(deleter, 'CallWith', 'ScriptState')
    is_ce_reactions = 'CEReactions' in extended_attributes
    return {
        'is_call_with_script_state': is_call_with_script_state,
        'is_ce_reactions': is_ce_reactions,
        'is_custom': 'Custom' in extended_attributes,
        'is_raises_exception': 'RaisesException' in extended_attributes,
        'name': cpp_name(deleter),
    }
