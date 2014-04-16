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

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

from collections import defaultdict

import idl_types
from idl_types import IdlType, inherits_interface
import v8_attributes
from v8_globals import includes
import v8_methods
import v8_types
from v8_types import cpp_ptr_type, cpp_template_type
import v8_utilities
from v8_utilities import capitalize, conditional_string, cpp_name, gc_type, has_extended_attribute_value, runtime_enabled_function_name


INTERFACE_H_INCLUDES = frozenset([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMWrapper.h',
    'bindings/v8/WrapperTypeInfo.h',
    'heap/Handle.h',
])
INTERFACE_CPP_INCLUDES = frozenset([
    'RuntimeEnabledFeatures.h',
    'bindings/v8/ExceptionState.h',
    'bindings/v8/V8DOMConfiguration.h',
    'bindings/v8/V8HiddenValue.h',
    'bindings/v8/V8ObjectConstructor.h',
    'core/dom/ContextFeatures.h',
    'core/dom/Document.h',
    'platform/TraceEvent.h',
    'wtf/GetPtr.h',
    'wtf/RefPtr.h',
])


def generate_interface(interface):
    includes.clear()
    includes.update(INTERFACE_CPP_INCLUDES)
    header_includes = set(INTERFACE_H_INCLUDES)

    parent_interface = interface.parent
    if parent_interface:
        header_includes.update(v8_types.includes_for_interface(parent_interface))
    extended_attributes = interface.extended_attributes

    is_audio_buffer = inherits_interface(interface.name, 'AudioBuffer')
    if is_audio_buffer:
        includes.add('modules/webaudio/AudioBuffer.h')

    is_document = inherits_interface(interface.name, 'Document')
    if is_document:
        includes.update(['bindings/v8/ScriptController.h',
                         'bindings/v8/V8WindowShell.h',
                         'core/frame/LocalFrame.h'])

    # [ActiveDOMObject]
    is_active_dom_object = 'ActiveDOMObject' in extended_attributes

    # [CheckSecurity]
    is_check_security = 'CheckSecurity' in extended_attributes
    if is_check_security:
        includes.add('bindings/v8/BindingSecurity.h')

    # [DependentLifetime]
    is_dependent_lifetime = 'DependentLifetime' in extended_attributes

    # [MeasureAs]
    is_measure_as = 'MeasureAs' in extended_attributes
    if is_measure_as:
        includes.add('core/frame/UseCounter.h')

    # [SetWrapperReferenceFrom]
    reachable_node_function = extended_attributes.get('SetWrapperReferenceFrom')
    if reachable_node_function:
        includes.update(['bindings/v8/V8GCController.h',
                         'core/dom/Element.h'])

    # [SetWrapperReferenceTo]
    set_wrapper_reference_to_list = [{
        'name': argument.name,
        # FIXME: properly should be:
        # 'cpp_type': argument.idl_type.cpp_type_args(used_as_argument=True),
        # (if type is non-wrapper type like NodeFilter, normally RefPtr)
        # Raw pointers faster though, and NodeFilter hacky anyway.
        'cpp_type': argument.idl_type.implemented_as + '*',
        'idl_type': argument.idl_type,
        'v8_type': v8_types.v8_type(argument.idl_type.name),
    } for argument in extended_attributes.get('SetWrapperReferenceTo', [])]
    for set_wrapper_reference_to in set_wrapper_reference_to_list:
        set_wrapper_reference_to['idl_type'].add_includes_for_type()

    # [SpecialWrapFor]
    if 'SpecialWrapFor' in extended_attributes:
        special_wrap_for = extended_attributes['SpecialWrapFor'].split('|')
    else:
        special_wrap_for = []
    for special_wrap_interface in special_wrap_for:
        v8_types.add_includes_for_interface(special_wrap_interface)

    # [Custom=Wrap], [SetWrapperReferenceFrom]
    has_visit_dom_wrapper = (
        has_extended_attribute_value(interface, 'Custom', 'VisitDOMWrapper') or
        reachable_node_function or
        set_wrapper_reference_to_list)

    this_gc_type = gc_type(interface)

    template_contents = {
        'conditional_string': conditional_string(interface),  # [Conditional]
        'cpp_class': cpp_name(interface),
        'gc_type': this_gc_type,
        'has_custom_legacy_call_as_function': has_extended_attribute_value(interface, 'Custom', 'LegacyCallAsFunction'),  # [Custom=LegacyCallAsFunction]
        'has_custom_to_v8': has_extended_attribute_value(interface, 'Custom', 'ToV8'),  # [Custom=ToV8]
        'has_custom_wrap': has_extended_attribute_value(interface, 'Custom', 'Wrap'),  # [Custom=Wrap]
        'has_visit_dom_wrapper': has_visit_dom_wrapper,
        'header_includes': header_includes,
        'interface_name': interface.name,
        'is_active_dom_object': is_active_dom_object,
        'is_audio_buffer': is_audio_buffer,
        'is_check_security': is_check_security,
        'is_dependent_lifetime': is_dependent_lifetime,
        'is_document': is_document,
        'is_event_target': inherits_interface(interface.name, 'EventTarget'),
        'is_exception': interface.is_exception,
        'is_node': inherits_interface(interface.name, 'Node'),
        'measure_as': v8_utilities.measure_as(interface),  # [MeasureAs]
        'parent_interface': parent_interface,
        'pass_cpp_type': cpp_template_type(
            cpp_ptr_type('PassRefPtr', 'RawPtr', this_gc_type),
            cpp_name(interface)),
        'reachable_node_function': reachable_node_function,
        'runtime_enabled_function': runtime_enabled_function_name(interface),  # [RuntimeEnabled]
        'set_wrapper_reference_to_list': set_wrapper_reference_to_list,
        'special_wrap_for': special_wrap_for,
        'v8_class': v8_utilities.v8_class_name(interface),
        'wrapper_configuration': 'WrapperConfiguration::Dependent'
            if (has_visit_dom_wrapper or
                is_active_dom_object or
                is_dependent_lifetime)
            else 'WrapperConfiguration::Independent',
    }

    # Constructors
    constructors = [generate_constructor(interface, constructor)
                    for constructor in interface.constructors
                    # FIXME: shouldn't put named constructors with constructors
                    # (currently needed for Perl compatibility)
                    # Handle named constructors separately
                    if constructor.name == 'Constructor']
    generate_constructor_overloads(constructors)

    # [CustomConstructor]
    custom_constructors = [{  # Only needed for computing interface length
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    } for constructor in interface.custom_constructors]

    # [EventConstructor]
    has_event_constructor = 'EventConstructor' in extended_attributes
    any_type_attributes = [attribute for attribute in interface.attributes
                           if attribute.idl_type.name == 'Any']
    if has_event_constructor:
        includes.add('bindings/v8/Dictionary.h')
        if any_type_attributes:
            includes.add('bindings/v8/SerializedScriptValue.h')

    # [NamedConstructor]
    named_constructor = generate_named_constructor(interface)

    if (constructors or custom_constructors or has_event_constructor or
        named_constructor):
        includes.add('bindings/v8/V8ObjectConstructor.h')
        includes.add('core/frame/DOMWindow.h')

    template_contents.update({
        'any_type_attributes': any_type_attributes,
        'constructors': constructors,
        'has_custom_constructor': bool(custom_constructors),
        'has_event_constructor': has_event_constructor,
        'interface_length':
            interface_length(interface, constructors + custom_constructors),
        'is_constructor_call_with_document': has_extended_attribute_value(
            interface, 'ConstructorCallWith', 'Document'),  # [ConstructorCallWith=Document]
        'is_constructor_call_with_execution_context': has_extended_attribute_value(
            interface, 'ConstructorCallWith', 'ExecutionContext'),  # [ConstructorCallWith=ExeuctionContext]
        'is_constructor_raises_exception': extended_attributes.get('RaisesException') == 'Constructor',  # [RaisesException=Constructor]
        'named_constructor': named_constructor,
    })

    # Constants
    template_contents.update({
        'constants': [generate_constant(constant) for constant in interface.constants],
        'do_not_check_constants': 'DoNotCheckConstants' in extended_attributes,
    })

    # Attributes
    attributes = [v8_attributes.generate_attribute(interface, attribute)
                  for attribute in interface.attributes]
    template_contents.update({
        'attributes': attributes,
        'has_accessors': any(attribute['is_expose_js_accessors'] for attribute in attributes),
        'has_attribute_configuration': any(
             not (attribute['is_expose_js_accessors'] or
                  attribute['is_static'] or
                  attribute['runtime_enabled_function'] or
                  attribute['per_context_enabled_function'])
             for attribute in attributes),
        'has_constructor_attributes': any(attribute['constructor_type'] for attribute in attributes),
        'has_per_context_enabled_attributes': any(attribute['per_context_enabled_function'] for attribute in attributes),
        'has_replaceable_attributes': any(attribute['is_replaceable'] for attribute in attributes),
    })

    # Methods
    methods = [v8_methods.generate_method(interface, method)
               for method in interface.operations
               if method.name]  # Skip anonymous special operations (methods)
    generate_overloads(methods)
    for method in methods:
        method['do_generate_method_configuration'] = (
            method['do_not_check_signature'] and
            not method['per_context_enabled_function'] and
            # For overloaded methods, only generate one accessor
            ('overload_index' not in method or method['overload_index'] == 1))

    template_contents.update({
        'has_origin_safe_method_setter': any(
            method['is_check_security_for_frame'] and not method['is_read_only']
            for method in methods),
        'has_method_configuration': any(method['do_generate_method_configuration'] for method in methods),
        'has_per_context_enabled_methods': any(method['per_context_enabled_function'] for method in methods),
        'methods': methods,
    })

    template_contents.update({
        'indexed_property_getter': indexed_property_getter(interface),
        'indexed_property_setter': indexed_property_setter(interface),
        'indexed_property_deleter': indexed_property_deleter(interface),
        'is_override_builtins': 'OverrideBuiltins' in extended_attributes,
        'named_property_getter': named_property_getter(interface),
        'named_property_setter': named_property_setter(interface),
        'named_property_deleter': named_property_deleter(interface),
    })

    return template_contents


# [DeprecateAs], [Reflect], [RuntimeEnabled]
def generate_constant(constant):
    # (Blink-only) string literals are unquoted in tokenizer, must be re-quoted
    # in C++.
    if constant.idl_type.name == 'String':
        value = '"%s"' % constant.value
    else:
        value = constant.value

    extended_attributes = constant.extended_attributes
    return {
        'cpp_class': extended_attributes.get('PartialInterfaceImplementedAs'),
        'name': constant.name,
        # FIXME: use 'reflected_name' as correct 'name'
        'reflected_name': extended_attributes.get('Reflect', constant.name),
        'runtime_enabled_function': runtime_enabled_function_name(constant),
        'value': value,
    }


################################################################################
# Overloads
################################################################################

def generate_overloads(methods):
    # Regular methods
    generate_overloads_by_type([method for method in methods
                                if not method['is_static']])
    # Static methods
    generate_overloads_by_type([method for method in methods
                                if method['is_static']])


def generate_overloads_by_type(methods):
    """Generates |method.overload*| template values.

    Modifies |method| in place for |method| in |methods|.
    Called separately for static and non-static (regular) methods,
    as these are overloaded separately.
    Doesn't change the |methods| list itself (only the values, i.e. individual
    methods), so ok to treat these separately.
    """

    # Once using Python 2.7, using collections.Counter
    # method_counts = Counter(method['name'] for method in methods)
    method_counts = defaultdict(lambda: 0)
    for method in methods:
        name = method['name']
        method_counts[name] += 1

    # Filter to only methods that are actually overloaded
    overloaded_method_counts = dict((name, count)
                                    for name, count in method_counts.iteritems()
                                    if count > 1)
    overloaded_name_methods = [(method['name'], method) for method in methods
                               if method['name'] in overloaded_method_counts]

    # Add overload information only to overloaded methods, so template code can
    # easily verify if a function is overloaded
    method_overloads = defaultdict(list)
    for name, method in overloaded_name_methods:
        # Overload index includes self, so first append, then compute index
        method_overloads[name].append(method)
        method.update({
            'overload_index': len(method_overloads[name]),
            'overload_resolution_expression': overload_resolution_expression(method),
        })

    # Resolution function is generated after last overloaded function;
    # package necessary information into |method.overloads| for that method.
    last_overloaded_name_methods = [
        (name, method) for name, method in overloaded_name_methods
        if method['overload_index'] == overloaded_method_counts[name]]
    for name, method in last_overloaded_name_methods:
        overloads = method_overloads[name]
        minimum_number_of_required_arguments = min(
            overload['number_of_required_arguments']
            for overload in overloads)
        method['overloads'] = {
            'has_exception_state': bool(minimum_number_of_required_arguments),
            'methods': overloads,
            'minimum_number_of_required_arguments': minimum_number_of_required_arguments,
            'name': name,
        }


def overload_resolution_expression(method):
    # Expression is an OR of ANDs: each term in the OR corresponds to a
    # possible argument count for a given method, with type checks.
    # FIXME: Blink's overload resolution algorithm is incorrect, per:
    # Implement WebIDL overload resolution algorithm.  http://crbug.com/293561
    #
    # Currently if distinguishing non-primitive type from primitive type,
    # (e.g., sequence<DOMString> from DOMString or Dictionary from double)
    # the method with a non-primitive type argument must appear *first* in the
    # IDL file, since we're not adding a check to primitive types.
    # FIXME: Once fixed, check IDLs, as usually want methods with primitive
    # types to appear first (style-wise).
    #
    # Properly:
    # 1. Compute effective overload set.
    # 2. First check type list length.
    # 3. If multiple entries for given length, compute distinguishing argument
    #    index and have check for that type.
    arguments = method['arguments']
    overload_checks = [overload_check_expression(method, index)
                       # check *omitting* optional arguments at |index| and up:
                       # index 0 => argument_count 0 (no arguments)
                       # index 1 => argument_count 1 (index 0 argument only)
                       for index, argument in enumerate(arguments)
                       if argument['is_optional']]
    # FIXME: this is wrong if a method has optional arguments and a variadic
    # one, though there are not yet any examples of this
    if not method['is_variadic']:
        # Includes all optional arguments (len = last index + 1)
        overload_checks.append(overload_check_expression(method, len(arguments)))
    return ' || '.join('(%s)' % check for check in overload_checks)


def overload_check_expression(method, argument_count):
    overload_checks = ['info.Length() == %s' % argument_count]
    arguments = method['arguments'][:argument_count]
    overload_checks.extend(overload_check_argument(index, argument)
                           for index, argument in
                           enumerate(arguments))
    return ' && '.join('(%s)' % check for check in overload_checks if check)


def overload_check_argument(index, argument):
    def null_or_optional_check():
        # If undefined is passed for an optional argument, the argument should
        # be treated as missing; otherwise undefined is not allowed.
        if idl_type.is_nullable:
            if argument['is_optional']:
                return 'isUndefinedOrNull(%s)'
            return '%s->IsNull()'
        if argument['is_optional']:
            return '%s->IsUndefined()'
        return None

    cpp_value = 'info[%s]' % index
    idl_type = argument['idl_type_object']
    # FIXME: proper type checking, sharing code with attributes and methods
    if idl_type.name == 'String' and argument['is_strict_type_checking']:
        return ' || '.join(['isUndefinedOrNull(%s)' % cpp_value,
                            '%s->IsString()' % cpp_value,
                            '%s->IsObject()' % cpp_value])
    if idl_type.array_or_sequence_type:
        return '%s->IsArray()' % cpp_value
    if idl_type.is_callback_interface:
        return ' || '.join(['%s->IsNull()' % cpp_value,
                            '%s->IsFunction()' % cpp_value])
    if idl_type.is_wrapper_type:
        type_check = 'V8{idl_type}::hasInstance({cpp_value}, info.GetIsolate())'.format(idl_type=idl_type.base_type, cpp_value=cpp_value)
        if idl_type.is_nullable:
            if argument['has_default']:
                type_check = ' || '.join(['isUndefinedOrNull(%s)' % cpp_value, type_check])
            else:
                type_check = ' || '.join(['%s->IsNull()' % cpp_value, type_check])
        return type_check
    if idl_type.is_interface_type:
        # Non-wrapper types are just objects: we don't distinguish type
        # We only allow undefined for non-wrapper types (notably Dictionary),
        # as we need it for optional Dictionary arguments, but we don't want to
        # change behavior of existing bindings for other types.
        type_check = '%s->IsObject()' % cpp_value
        added_check_template = null_or_optional_check()
        if added_check_template:
            type_check = ' || '.join([added_check_template % cpp_value,
                                      type_check])
        return type_check
    return None


################################################################################
# Constructors
################################################################################

# [Constructor]
def generate_constructor(interface, constructor):
    return {
        'argument_list': constructor_argument_list(interface, constructor),
        'arguments': [constructor_argument(interface, constructor, argument, index)
                      for index, argument in enumerate(constructor.arguments)],
        'cpp_type': cpp_template_type(
            cpp_ptr_type('RefPtr', 'RawPtr', gc_type(interface)),
            cpp_name(interface)),
        'has_exception_state':
            # [RaisesException=Constructor]
            interface.extended_attributes.get('RaisesException') == 'Constructor' or
            any(argument for argument in constructor.arguments
                if argument.idl_type.name == 'SerializedScriptValue' or
                   argument.idl_type.is_integer_type),
        'is_constructor': True,
        'is_named_constructor': False,
        'is_variadic': False,  # Required for overload resolution
        'number_of_required_arguments':
            number_of_required_arguments(constructor),
    }


def constructor_argument_list(interface, constructor):
    arguments = []
    # [ConstructorCallWith=ExecutionContext]
    if has_extended_attribute_value(interface, 'ConstructorCallWith', 'ExecutionContext'):
        arguments.append('context')
    # [ConstructorCallWith=Document]
    if has_extended_attribute_value(interface, 'ConstructorCallWith', 'Document'):
        arguments.append('document')

    arguments.extend([argument.name for argument in constructor.arguments])

    # [RaisesException=Constructor]
    if interface.extended_attributes.get('RaisesException') == 'Constructor':
        arguments.append('exceptionState')

    return arguments


def constructor_argument(interface, constructor, argument, index):
    idl_type = argument.idl_type
    return {
        'cpp_value':
            v8_methods.cpp_value(interface, constructor, index),
        'has_default': 'Default' in argument.extended_attributes,
        # Dictionary is special-cased, but arrays and sequences shouldn't be
        'idl_type': not idl_type.array_or_sequence_type and idl_type.base_type,
        'idl_type_object': idl_type,
        'index': index,
        'is_optional': argument.is_optional,
        'is_strict_type_checking': False,  # Required for overload resolution
        'name': argument.name,
        'v8_value_to_local_cpp_value':
            v8_methods.v8_value_to_local_cpp_value(argument, index),
    }


def generate_constructor_overloads(constructors):
    if len(constructors) <= 1:
        return
    for overload_index, constructor in enumerate(constructors):
        constructor.update({
            'overload_index': overload_index + 1,
            'overload_resolution_expression':
                overload_resolution_expression(constructor),
        })


# [NamedConstructor]
def generate_named_constructor(interface):
    extended_attributes = interface.extended_attributes
    if 'NamedConstructor' not in extended_attributes:
        return None
    # FIXME: parser should return named constructor separately;
    # included in constructors (and only name stored in extended attribute)
    # for Perl compatibility
    idl_constructor = interface.constructors[0]
    constructor = generate_constructor(interface, idl_constructor)
    constructor['argument_list'].insert(0, '*document')
    constructor.update({
        'name': extended_attributes['NamedConstructor'],
        'is_named_constructor': True,
    })
    return constructor


def number_of_required_arguments(constructor):
    return len([argument for argument in constructor.arguments
                if not argument.is_optional])


def interface_length(interface, constructors):
    # Docs: http://heycam.github.io/webidl/#es-interface-call
    if 'EventConstructor' in interface.extended_attributes:
        return 1
    if not constructors:
        return 0
    return min(constructor['number_of_required_arguments']
               for constructor in constructors)


################################################################################
# Special operations (methods)
# http://heycam.github.io/webidl/#idl-special-operations
################################################################################

def property_getter(getter, cpp_arguments):
    def is_null_expression(idl_type):
        if idl_type.is_union_type:
            return ' && '.join('!result%sEnabled' % i
                               for i, _ in enumerate(idl_type.member_types))
        if idl_type.name == 'String':
            return 'result.isNull()'
        if idl_type.is_interface_type:
            return '!result'
        return ''

    idl_type = getter.idl_type
    extended_attributes = getter.extended_attributes
    is_raises_exception = 'RaisesException' in extended_attributes

    # FIXME: make more generic, so can use v8_methods.cpp_value
    cpp_method_name = 'impl->%s' % cpp_name(getter)

    if is_raises_exception:
        cpp_arguments.append('exceptionState')
    union_arguments = idl_type.union_arguments
    if union_arguments:
        cpp_arguments.extend(union_arguments)

    cpp_value = '%s(%s)' % (cpp_method_name, ', '.join(cpp_arguments))

    return {
        'cpp_type': idl_type.cpp_type,
        'cpp_value': cpp_value,
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
        'union_arguments': union_arguments,
        'v8_set_return_value': idl_type.v8_set_return_value('result', extended_attributes=extended_attributes, script_wrappable='impl', release=idl_type.release),
    }


def property_setter(setter):
    idl_type = setter.arguments[1].idl_type
    extended_attributes = setter.extended_attributes
    is_raises_exception = 'RaisesException' in extended_attributes
    return {
        'has_strict_type_checking':
            'StrictTypeChecking' in extended_attributes and
            idl_type.is_wrapper_type,
        'idl_type': idl_type.base_type,
        'is_custom': 'Custom' in extended_attributes,
        'has_exception_state': is_raises_exception or
                               idl_type.is_integer_type,
        'is_raises_exception': is_raises_exception,
        'name': cpp_name(setter),
        'v8_value_to_local_cpp_value': idl_type.v8_value_to_local_cpp_value(
            extended_attributes, 'v8Value', 'propertyValue'),
    }


def property_deleter(deleter):
    idl_type = deleter.idl_type
    if str(idl_type) != 'boolean':
        raise Exception(
            'Only deleters with boolean type are allowed, but type is "%s"' %
            idl_type)
    extended_attributes = deleter.extended_attributes
    return {
        'is_custom': 'Custom' in extended_attributes,
        'is_raises_exception': 'RaisesException' in extended_attributes,
        'name': cpp_name(deleter),
    }


################################################################################
# Indexed properties
# http://heycam.github.io/webidl/#idl-indexed-properties
################################################################################

def indexed_property_getter(interface):
    try:
        # Find indexed property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1)
        getter = next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_getter(getter, ['index'])


def indexed_property_setter(interface):
    try:
        # Find indexed property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1, ARG_TYPE ARG2)
        setter = next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_setter(setter)


def indexed_property_deleter(interface):
    try:
        # Find indexed property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG)
        deleter = next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None

    return property_deleter(deleter)


################################################################################
# Named properties
# http://heycam.github.io/webidl/#idl-named-properties
################################################################################

def named_property_getter(interface):
    try:
        # Find named property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1)
        getter = next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    getter.name = getter.name or 'anonymousNamedGetter'
    return property_getter(getter, ['propertyName'])


def named_property_setter(interface):
    try:
        # Find named property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1, ARG_TYPE ARG2)
        setter = next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    return property_setter(setter)


def named_property_deleter(interface):
    try:
        # Find named property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG)
        deleter = next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None

    return property_deleter(deleter)
