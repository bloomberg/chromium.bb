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

"""Functions for type handling and type conversion (Blink/C++ <-> V8/JS).

Spec:
http://www.w3.org/TR/WebIDL/#es-type-mapping

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import posixpath
import re

from v8_globals import includes
import idl_definitions  # for UnionType
from v8_utilities import strip_suffix

################################################################################
# IDL types
################################################################################

BASIC_TYPES = set([
    # Built-in, non-composite, non-object data types
    # http://www.w3.org/TR/WebIDL/#dfn-primitive-type
    'boolean',
    'float',
    # unrestricted float is not supported
    'double',
    # unrestricted double is not supported
    # http://www.w3.org/TR/WebIDL/#idl-types
    'DOMString',
    'Date',
    # http://www.w3.org/TR/WebIDL/#es-type-mapping
    'void',
])
INTEGER_TYPES = set([
    # http://www.w3.org/TR/WebIDL/#dfn-integer-type
    'byte',
    'octet',
    'short',
    'unsigned short',
    # int and unsigned are not IDL types
    'long',
    'unsigned long',
    'long long',
    'unsigned long long',
])
BASIC_TYPES.update(INTEGER_TYPES)

ancestors = {}  # interface_name -> ancestors
callback_functions = set()
callback_interfaces = set()
enums = {}  # name -> values


def array_or_sequence_type(idl_type):
    return array_type(idl_type) or sequence_type(idl_type)


def array_type(idl_type):
    if is_union_type(idl_type):
        # We do not support arrays of union types
        return False
    matched = re.match(r'([\w\s]+)\[\]', idl_type)
    return matched and matched.group(1)


def is_basic_type(idl_type):
    return idl_type in BASIC_TYPES


def is_integer_type(idl_type):
    return idl_type in INTEGER_TYPES


def is_callback_function(idl_type):
    return idl_type in callback_functions


def set_callback_functions(new_callback_functions):
    callback_functions.update(new_callback_functions)


def is_callback_interface(idl_type):
    return idl_type in callback_interfaces


def set_callback_interfaces(new_callback_interfaces):
    callback_interfaces.update(new_callback_interfaces)


def is_composite_type(idl_type):
    return (idl_type == 'any' or
            array_type(idl_type) or
            sequence_type(idl_type) or
            is_union_type(idl_type))


def is_enum(idl_type):
    return idl_type in enums


def enum_values(idl_type):
    return enums.get(idl_type)


def set_enums(new_enums):
    enums.update(new_enums)


def inherits_interface(interface_name, ancestor_name):
    return (interface_name == ancestor_name or
            ancestor_name in ancestors.get(interface_name, []))


def set_ancestors(new_ancestors):
    ancestors.update(new_ancestors)


def is_interface_type(idl_type):
    # Anything that is not another type is an interface type.
    # http://www.w3.org/TR/WebIDL/#idl-types
    # http://www.w3.org/TR/WebIDL/#idl-interface
    # In C++ these are RefPtr or PassRefPtr types.
    return not(is_basic_type(idl_type) or
               is_composite_type(idl_type) or
               is_callback_function(idl_type) or
               is_enum(idl_type) or
               idl_type == 'object' or
               idl_type == 'Promise')  # Promise will be basic in future


def sequence_type(idl_type):
    if is_union_type(idl_type):
        # We do not support sequences of union types
        return False
    matched = re.match(r'sequence<([\w\s]+)>', idl_type)
    return matched and matched.group(1)


def is_union_type(idl_type):
    return isinstance(idl_type, idl_definitions.IdlUnionType)


################################################################################
# V8-specific type handling
################################################################################

NON_WRAPPER_TYPES = set([
    'CompareHow',
    'Dictionary',
    'EventHandler',
    'EventListener',
    'MediaQueryListListener',
    'NodeFilter',
    'SerializedScriptValue',
])
TYPED_ARRAYS = {
    # (cpp_type, v8_type), used by constructor templates
    'ArrayBuffer': None,
    'ArrayBufferView': None,
    'Float32Array': ('float', 'v8::kExternalFloatArray'),
    'Float64Array': ('double', 'v8::kExternalDoubleArray'),
    'Int8Array': ('signed char', 'v8::kExternalByteArray'),
    'Int16Array': ('short', 'v8::kExternalShortArray'),
    'Int32Array': ('int', 'v8::kExternalIntArray'),
    'Uint8Array': ('unsigned char', 'v8::kExternalUnsignedByteArray'),
    'Uint8ClampedArray': ('unsigned char', 'v8::kExternalPixelArray'),
    'Uint16Array': ('unsigned short', 'v8::kExternalUnsignedShortArray'),
    'Uint32Array': ('unsigned int', 'v8::kExternalUnsignedIntArray'),
}


def constructor_type(idl_type):
    return strip_suffix(idl_type, 'Constructor')


def is_typed_array_type(idl_type):
    return idl_type in TYPED_ARRAYS


def is_wrapper_type(idl_type):
    return (is_interface_type(idl_type) and
            idl_type not in NON_WRAPPER_TYPES)


################################################################################
# C++ types
################################################################################

CPP_TYPE_SAME_AS_IDL_TYPE = set([
    'double',
    'float',
    'long long',
    'unsigned long long',
])
CPP_INT_TYPES = set([
    'byte',
    'long',
    'short',
])
CPP_UNSIGNED_TYPES = set([
    'octet',
    'unsigned int',
    'unsigned long',
    'unsigned short',
])
CPP_SPECIAL_CONVERSION_RULES = {
    'CompareHow': 'Range::CompareHow',
    'Date': 'double',
    'Dictionary': 'Dictionary',
    'EventHandler': 'EventListener*',
    'Promise': 'ScriptPromise',
    'ScriptValue': 'ScriptValue',
    'boolean': 'bool',
}

def cpp_type(idl_type, extended_attributes=None, used_as_argument=False):
    """Returns C++ type corresponding to IDL type."""
    def string_mode():
        # FIXME: the Web IDL spec requires 'EmptyString', not 'NullString',
        # but we use NullString for performance.
        if extended_attributes.get('TreatNullAs') != 'NullString':
            return ''
        if extended_attributes.get('TreatUndefinedAs') != 'NullString':
            return 'WithNullCheck'
        return 'WithUndefinedOrNullCheck'

    extended_attributes = extended_attributes or {}
    idl_type = preprocess_idl_type(idl_type)

    if idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return idl_type
    if idl_type in CPP_INT_TYPES:
        return 'int'
    if idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if idl_type in CPP_SPECIAL_CONVERSION_RULES:
        return CPP_SPECIAL_CONVERSION_RULES[idl_type]
    if (idl_type in NON_WRAPPER_TYPES or
        idl_type == 'XPathNSResolver'):  # FIXME: eliminate this special case
        return 'RefPtr<%s>' % idl_type
    if idl_type == 'DOMString':
        if not used_as_argument:
            return 'String'
        return 'V8StringResource<%s>' % string_mode()
    if is_union_type(idl_type):
        # Attribute 'union_member_types' use is ok, but pylint can't infer this
        # pylint: disable=E1103
        return (cpp_type(union_member_type)
                for union_member_type in idl_type.union_member_types)
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return cpp_template_type('Vector', cpp_type(this_array_or_sequence_type))

    if is_typed_array_type(idl_type) and used_as_argument:
        return idl_type + '*'
    if is_interface_type(idl_type):
        implemented_as_class = implemented_as(idl_type)
        if used_as_argument:
            return implemented_as_class + '*'
        if is_will_be_garbage_collected(idl_type):
            return cpp_template_type('RefPtrWillBeRawPtr', implemented_as_class)
        return cpp_template_type('RefPtr', implemented_as_class)
    # Default, assume native type is a pointer with same type name as idl type
    return idl_type + '*'


def cpp_template_type(template, inner_type):
    """Returns C++ template specialized to type, with space added if needed."""
    if inner_type.endswith('>'):
        format_string = '{template}<{inner_type} >'
    else:
        format_string = '{template}<{inner_type}>'
    return format_string.format(template=template, inner_type=inner_type)


def v8_type(interface_type):
    return 'V8' + interface_type


# [ImplementedAs]
# This handles [ImplementedAs] on interface types, not [ImplementedAs] in the
# interface being generated. e.g., given:
#   Foo.idl: interface Foo {attribute Bar bar};
#   Bar.idl: [ImplementedAs=Zork] interface Bar {};
# when generating bindings for Foo, the [ImplementedAs] on Bar is needed.
# This data is external to Foo.idl, and hence computed as global information in
# compute_dependencies.py to avoid having to parse IDLs of all used interfaces.
implemented_as_interfaces = {}


def implemented_as(idl_type):
    if idl_type in implemented_as_interfaces:
        return implemented_as_interfaces[idl_type]
    return idl_type


def set_implemented_as_interfaces(new_implemented_as_interfaces):
    implemented_as_interfaces.update(new_implemented_as_interfaces)


# [WillBeGarbageCollected]
will_be_garbage_collected_types = set()


def is_will_be_garbage_collected(idl_type):
    return idl_type in will_be_garbage_collected_types


def set_will_be_garbage_collected_types(new_will_be_garbage_collected_types):
    will_be_garbage_collected_types.update(new_will_be_garbage_collected_types)


################################################################################
# Includes
################################################################################

def includes_for_cpp_class(class_name, relative_dir_posix):
    return set([posixpath.join('bindings', relative_dir_posix, class_name + '.h')])


INCLUDES_FOR_TYPE = {
    'object': set(),
    'CompareHow': set(),
    'Dictionary': set(['bindings/v8/Dictionary.h']),
    'EventHandler': set(['bindings/v8/V8AbstractEventListener.h',
                         'bindings/v8/V8EventListenerList.h']),
    'EventListener': set(['bindings/v8/BindingSecurity.h',
                          'bindings/v8/V8EventListenerList.h',
                          'core/frame/DOMWindow.h']),
    'MediaQueryListListener': set(['core/css/MediaQueryListListener.h']),
    'Promise': set(['bindings/v8/ScriptPromise.h']),
    'SerializedScriptValue': set(['bindings/v8/SerializedScriptValue.h']),
    'ScriptValue': set(['bindings/v8/ScriptValue.h']),
}

def includes_for_type(idl_type):
    idl_type = preprocess_idl_type(idl_type)
    if idl_type in INCLUDES_FOR_TYPE:
        return INCLUDES_FOR_TYPE[idl_type]
    if is_basic_type(idl_type):
        return set()
    if is_typed_array_type(idl_type):
        return set(['bindings/v8/custom/V8%sCustom.h' % idl_type])
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return includes_for_type(this_array_or_sequence_type)
    if is_union_type(idl_type):
        # Attribute 'union_member_types' use is ok, but pylint can't infer this
        # pylint: disable=E1103
        return set.union(*[
            includes_for_type(union_member_type)
            for union_member_type in idl_type.union_member_types])
    if idl_type.endswith('ConstructorConstructor'):
        # FIXME: rename to NamedConstructor
        # Ending with 'ConstructorConstructor' indicates a named constructor,
        # and these do not have header files, as they are part of the generated
        # bindings for the interface
        return set()
    if idl_type.endswith('Constructor'):
        idl_type = constructor_type(idl_type)
    return set(['V8%s.h' % idl_type])


def add_includes_for_type(idl_type):
    includes.update(includes_for_type(idl_type))


################################################################################
# V8 -> C++
################################################################################

V8_VALUE_TO_CPP_VALUE = {
    # Basic
    'Date': 'toCoreDate({v8_value})',
    'DOMString': '{v8_value}',
    'boolean': '{v8_value}->BooleanValue()',
    'float': 'static_cast<float>({v8_value}->NumberValue())',
    'double': 'static_cast<double>({v8_value}->NumberValue())',
    'byte': 'toInt8({arguments})',
    'octet': 'toUInt8({arguments})',
    'short': 'toInt16({arguments})',
    'unsigned short': 'toUInt16({arguments})',
    'long': 'toInt32({arguments})',
    'unsigned long': 'toUInt32({arguments})',
    'long long': 'toInt64({arguments})',
    'unsigned long long': 'toUInt64({arguments})',
    # Interface types
    'CompareHow': 'static_cast<Range::CompareHow>({v8_value}->Int32Value())',
    'Dictionary': 'Dictionary({v8_value}, info.GetIsolate())',
    'EventTarget': 'V8DOMWrapper::isDOMWrapper({v8_value}) ? toWrapperTypeInfo(v8::Handle<v8::Object>::Cast({v8_value}))->toEventTarget(v8::Handle<v8::Object>::Cast({v8_value})) : 0',
    'MediaQueryListListener': 'MediaQueryListListener::create(ScriptValue({v8_value}, info.GetIsolate()))',
    'NodeFilter': 'toNodeFilter({v8_value}, info.GetIsolate())',
    'Promise': 'ScriptPromise({v8_value}, info.GetIsolate())',
    'SerializedScriptValue': 'SerializedScriptValue::create({v8_value}, info.GetIsolate())',
    'ScriptValue': 'ScriptValue({v8_value}, info.GetIsolate())',
    'Window': 'toDOMWindow({v8_value}, info.GetIsolate())',
    'XPathNSResolver': 'toXPathNSResolver({v8_value}, info.GetIsolate())',
}


def v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index):
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return v8_value_to_cpp_value_array_or_sequence(this_array_or_sequence_type, v8_value, index)

    idl_type = preprocess_idl_type(idl_type)
    add_includes_for_type(idl_type)

    if 'EnforceRange' in extended_attributes:
        arguments = ', '.join([v8_value, 'EnforceRange', 'exceptionState'])
    elif is_integer_type(idl_type):  # NormalConversion
        arguments = ', '.join([v8_value, 'exceptionState'])
    else:
        arguments = v8_value

    if idl_type in V8_VALUE_TO_CPP_VALUE:
        cpp_expression_format = V8_VALUE_TO_CPP_VALUE[idl_type]
    elif is_typed_array_type(idl_type):
        cpp_expression_format = (
            '{v8_value}->Is{idl_type}() ? '
            'V8{idl_type}::toNative(v8::Handle<v8::{idl_type}>::Cast({v8_value})) : 0')
    else:
        cpp_expression_format = (
            'V8{idl_type}::toNativeWithTypeCheck(info.GetIsolate(), {v8_value})')

    return cpp_expression_format.format(arguments=arguments, idl_type=idl_type, v8_value=v8_value)


def v8_value_to_cpp_value_array_or_sequence(this_array_or_sequence_type, v8_value, index):
    # Index is None for setters, index (starting at 0) for method arguments,
    # and is used to provide a human-readable exception message
    if index is None:
        index = 0  # special case, meaning "setter"
    else:
        index += 1  # human-readable index
    if (is_interface_type(this_array_or_sequence_type) and
        this_array_or_sequence_type != 'Dictionary'):
        this_cpp_type = None
        expression_format = '(toRefPtrNativeArray<{array_or_sequence_type}, V8{array_or_sequence_type}>({v8_value}, {index}, info.GetIsolate()))'
        add_includes_for_type(this_array_or_sequence_type)
    else:
        this_cpp_type = cpp_type(this_array_or_sequence_type)
        expression_format = 'toNativeArray<{cpp_type}>({v8_value}, {index}, info.GetIsolate())'
    expression = expression_format.format(array_or_sequence_type=this_array_or_sequence_type, cpp_type=this_cpp_type, index=index, v8_value=v8_value)
    return expression


def v8_value_to_local_cpp_value(idl_type, extended_attributes, v8_value, variable_name, index=None):
    """Returns an expression that converts a V8 value to a C++ value and stores it as a local value."""
    this_cpp_type = cpp_type(idl_type, extended_attributes=extended_attributes, used_as_argument=True)

    idl_type = preprocess_idl_type(idl_type)
    if idl_type == 'DOMString':
        format_string = 'V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID({cpp_type}, {variable_name}, {cpp_value})'
    elif is_integer_type(idl_type):
        format_string = 'V8TRYCATCH_EXCEPTION_VOID({cpp_type}, {variable_name}, {cpp_value}, exceptionState)'
    else:
        format_string = 'V8TRYCATCH_VOID({cpp_type}, {variable_name}, {cpp_value})'

    cpp_value = v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, index)
    return format_string.format(cpp_type=this_cpp_type, cpp_value=cpp_value, variable_name=variable_name)


################################################################################
# C++ -> V8
################################################################################

def preprocess_idl_type(idl_type):
    if is_enum(idl_type):
        # Enumerations are internally DOMStrings
        return 'DOMString'
    if (idl_type == 'any' or is_callback_function(idl_type)):
        return 'ScriptValue'
    return idl_type


def preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes):
    """Returns IDL type and value, with preliminary type conversions applied."""
    idl_type = preprocess_idl_type(idl_type)
    if idl_type == 'Promise':
        idl_type = 'ScriptValue'
    if idl_type in ['long long', 'unsigned long long']:
        # long long and unsigned long long are not representable in ECMAScript;
        # we represent them as doubles.
        idl_type = 'double'
        cpp_value = 'static_cast<double>(%s)' % cpp_value
    # HTML5 says that unsigned reflected attributes should be in the range
    # [0, 2^31). When a value isn't in this range, a default value (or 0)
    # should be returned instead.
    extended_attributes = extended_attributes or {}
    if ('Reflect' in extended_attributes and
        idl_type in ['unsigned long', 'unsigned short']):
        cpp_value = cpp_value.replace('getUnsignedIntegralAttribute',
                                      'getIntegralAttribute')
        cpp_value = 'std::max(0, %s)' % cpp_value
    return idl_type, cpp_value


def v8_conversion_type(idl_type, extended_attributes):
    """Returns V8 conversion type, adding any additional includes.

    The V8 conversion type is used to select the C++ -> V8 conversion function
    or v8SetReturnValue* function; it can be an idl_type, a cpp_type, or a
    separate name for the type of conversion (e.g., 'DOMWrapper').
    """
    extended_attributes = extended_attributes or {}
    # Basic types, without additional includes
    if idl_type in CPP_INT_TYPES:
        return 'int'
    if idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if idl_type == 'DOMString':
        if 'TreatReturnedNullStringAs' not in extended_attributes:
            return 'DOMString'
        treat_returned_null_string_as = extended_attributes['TreatReturnedNullStringAs']
        if treat_returned_null_string_as == 'Null':
            return 'StringOrNull'
        if treat_returned_null_string_as == 'Undefined':
            return 'StringOrUndefined'
        raise 'Unrecognized TreatReturnNullStringAs value: "%s"' % treat_returned_null_string_as
    if is_basic_type(idl_type) or idl_type == 'ScriptValue':
        return idl_type

    # Data type with potential additional includes
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        if is_interface_type(this_array_or_sequence_type):
            add_includes_for_type(this_array_or_sequence_type)
        return 'array'

    add_includes_for_type(idl_type)
    if idl_type in V8_SET_RETURN_VALUE:  # Special v8SetReturnValue treatment
        return idl_type

    # Pointer type
    return 'DOMWrapper'


V8_SET_RETURN_VALUE = {
    'boolean': 'v8SetReturnValueBool(info, {cpp_value})',
    'int': 'v8SetReturnValueInt(info, {cpp_value})',
    'unsigned': 'v8SetReturnValueUnsigned(info, {cpp_value})',
    'DOMString': 'v8SetReturnValueString(info, {cpp_value}, info.GetIsolate())',
    # [TreatNullReturnValueAs]
    'StringOrNull': 'v8SetReturnValueStringOrNull(info, {cpp_value}, info.GetIsolate())',
    'StringOrUndefined': 'v8SetReturnValueStringOrUndefined(info, {cpp_value}, info.GetIsolate())',
    'void': '',
    # No special v8SetReturnValue* function (set value directly)
    'float': 'v8SetReturnValue(info, {cpp_value})',
    'double': 'v8SetReturnValue(info, {cpp_value})',
    # No special v8SetReturnValue* function, but instead convert value to V8
    # and then use general v8SetReturnValue.
    'array': 'v8SetReturnValue(info, {cpp_value})',
    'Date': 'v8SetReturnValue(info, {cpp_value})',
    'EventHandler': 'v8SetReturnValue(info, {cpp_value})',
    'ScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    'SerializedScriptValue': 'v8SetReturnValue(info, {cpp_value})',
    # DOMWrapper
    'DOMWrapperForMainWorld': 'v8SetReturnValueForMainWorld(info, {cpp_value})',
    'DOMWrapperFast': 'v8SetReturnValueFast(info, {cpp_value}, {script_wrappable})',
    'DOMWrapperDefault': 'v8SetReturnValue(info, {cpp_value})',
}


def v8_set_return_value(idl_type, cpp_value, extended_attributes=None, script_wrappable='', release=False, for_main_world=False):
    """Returns a statement that converts a C++ value to a V8 value and sets it as a return value.

    release: for union types, can be either False (False for all member types)
             or a sequence (list or tuple) of booleans (if specified
             individually).
    """
    def dom_wrapper_conversion_type():
        if not script_wrappable:
            return 'DOMWrapperDefault'
        if for_main_world:
            return 'DOMWrapperForMainWorld'
        return 'DOMWrapperFast'

    if is_union_type(idl_type):
        return [
            v8_set_return_value(union_member_type,
                                cpp_value + str(i),
                                extended_attributes,
                                script_wrappable,
                                release and release[i])
                for i, union_member_type in
                enumerate(idl_type.union_member_types)]
    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes)
    # SetReturn-specific overrides
    if this_v8_conversion_type in ['Date', 'EventHandler', 'ScriptValue', 'SerializedScriptValue', 'array']:
        # Convert value to V8 and then use general v8SetReturnValue
        cpp_value = cpp_value_to_v8_value(idl_type, cpp_value, extended_attributes=extended_attributes)
    if this_v8_conversion_type == 'DOMWrapper':
        this_v8_conversion_type = dom_wrapper_conversion_type()

    format_string = V8_SET_RETURN_VALUE[this_v8_conversion_type]
    if release:
        cpp_value = '%s.release()' % cpp_value
    statement = format_string.format(cpp_value=cpp_value, script_wrappable=script_wrappable)
    return statement


CPP_VALUE_TO_V8_VALUE = {
    # Built-in types
    'Date': 'v8DateOrNull({cpp_value}, {isolate})',
    'DOMString': 'v8String({isolate}, {cpp_value})',
    'boolean': 'v8Boolean({cpp_value}, {isolate})',
    'int': 'v8::Integer::New({isolate}, {cpp_value})',
    'unsigned': 'v8::Integer::NewFromUnsigned({isolate}, {cpp_value})',
    'float': 'v8::Number::New({isolate}, {cpp_value})',
    'double': 'v8::Number::New({isolate}, {cpp_value})',
    'void': 'v8Undefined()',
    # Special cases
    'EventHandler': '{cpp_value} ? v8::Handle<v8::Value>(V8AbstractEventListener::cast({cpp_value})->getListenerObject(imp->executionContext())) : v8::Handle<v8::Value>(v8::Null({isolate}))',
    'ScriptValue': '{cpp_value}.v8Value()',
    'SerializedScriptValue': '{cpp_value} ? {cpp_value}->deserialize() : v8::Handle<v8::Value>(v8::Null({isolate}))',
    # General
    'array': 'v8Array({cpp_value}, {isolate})',
    'DOMWrapper': 'toV8({cpp_value}, {creation_context}, {isolate})',
}


def cpp_value_to_v8_value(idl_type, cpp_value, isolate='info.GetIsolate()', creation_context='', extended_attributes=None):
    """Returns an expression that converts a C++ value to a V8 value."""
    # the isolate parameter is needed for callback interfaces
    idl_type, cpp_value = preprocess_idl_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes)
    format_string = CPP_VALUE_TO_V8_VALUE[this_v8_conversion_type]
    statement = format_string.format(cpp_value=cpp_value, isolate=isolate, creation_context=creation_context)
    return statement
