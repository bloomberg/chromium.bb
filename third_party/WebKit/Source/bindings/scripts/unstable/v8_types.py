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

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

import posixpath
import re

import idl_definitions  # for UnionType

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
    # integer types
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
    # http://www.w3.org/TR/WebIDL/#idl-types
    'DOMString',
    'Date',
    # http://www.w3.org/TR/WebIDL/#es-type-mapping
    'void',
])

enum_types = {}  # name -> values
callback_function_types = set()


def array_or_sequence_type(idl_type):
    return array_type(idl_type) or sequence_type(idl_type)


def array_type(idl_type):
    matched = re.match(r'([\w\s]+)\[\]', idl_type)
    return matched and matched.group(1)


def is_basic_type(idl_type):
    return idl_type in BASIC_TYPES


def is_callback_function_type(idl_type):
    return idl_type in callback_function_types


def set_callback_function_types(callback_functions):
    callback_function_types.update(callback_functions.keys())


def is_composite_type(idl_type):
    return (idl_type == 'any' or
            array_type(idl_type) or
            sequence_type(idl_type) or
            is_union_type(idl_type))


def is_enum_type(idl_type):
    return idl_type in enum_types


def enum_values(idl_type):
    return enum_types.get(idl_type)


def set_enum_types(enumerations):
    enum_types.update([[enum.name, enum.values]
                       for enum in enumerations.values()])


def is_interface_type(idl_type):
    # Anything that is not another type is an interface type.
    # http://www.w3.org/TR/WebIDL/#idl-types
    # http://www.w3.org/TR/WebIDL/#idl-interface
    # In C++ these are RefPtr or PassRefPtr types.
    return not(is_basic_type(idl_type) or
               is_composite_type(idl_type) or
               is_callback_function_type(idl_type) or
               is_enum_type(idl_type) or
               idl_type == 'object' or
               idl_type == 'Promise')  # Promise will be basic in future


def sequence_type(idl_type):
    matched = re.match(r'sequence<([\w\s]+)>', idl_type)
    return matched and matched.group(1)


def is_union_type(idl_type):
    return isinstance(idl_type, idl_definitions.IdlUnionType)


################################################################################
# V8-specific type handling
################################################################################

DOM_NODE_TYPES = set([
    'Attr',
    'CDATASection',
    'CharacterData',
    'Comment',
    'Document',
    'DocumentFragment',
    'DocumentType',
    'Element',
    'Entity',
    'HTMLDocument',
    'Node',
    'Notation',
    'ProcessingInstruction',
    'ShadowRoot',
    'SVGDocument',
    'Text',
    'TestNode',
])
NON_WRAPPER_TYPES = set([
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


def is_dom_node_type(idl_type):
    return (idl_type in DOM_NODE_TYPES or
            (idl_type.startswith(('HTML', 'SVG')) and
             idl_type.endswith('Element')))


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
    'Date': 'double',
    'Promise': 'ScriptPromise',
    'any': 'ScriptValue',
    'boolean': 'bool',
}
CPP_REF_PTR_CONVERSION_TYPES = set([
    # FIXME: overlaps with NON_WRAPPER_TYPES
    # FIXME: other special cases occur only for methods (not attributes)
    'DOMStringList',
    'NodeFilter',
    'SerializedScriptValue',
])

def cpp_type(idl_type, extended_attributes=None, used_as_argument=False):
    """Returns C++ type corresponding to IDL type."""
    extended_attributes = extended_attributes or {}
    if idl_type in CPP_TYPE_SAME_AS_IDL_TYPE:
        return idl_type
    if idl_type in CPP_INT_TYPES:
        return 'int'
    if idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if idl_type in CPP_SPECIAL_CONVERSION_RULES:
        return CPP_SPECIAL_CONVERSION_RULES[idl_type]
    if idl_type in CPP_REF_PTR_CONVERSION_TYPES:
        return 'RefPtr<%s>' % idl_type
    if idl_type == 'DOMString' or is_enum_type(idl_type):
        # enum is internally a string
        if used_as_argument:
            return 'V8StringResource<>'
        return 'String'
    if is_callback_function_type(idl_type):
        return 'ScriptValue'
    if is_union_type(idl_type):
        raise Exception('UnionType is not supported')
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return cpp_template_type('Vector', cpp_type(this_array_or_sequence_type))

    # Special cases
    if idl_type == 'EventHandler':
        return 'EventListener*'

    # FIXME: fix Perl code reading:
    # return "RefPtr<${type}>" if IsRefPtrType($type) and not $isParameter;
    if is_interface_type(idl_type):
        if used_as_argument:
            return cpp_template_type('PassRefPtr', idl_type)
        return cpp_template_type('RefPtr', idl_type)
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


################################################################################
# Includes
################################################################################


def includes_for_cpp_class(class_name, relative_dir_posix):
    return set([posixpath.join('bindings', relative_dir_posix, class_name + '.h')])


def skip_includes(idl_type):
    return (is_basic_type(idl_type) or
            is_callback_function_type(idl_type) or
            is_enum_type(idl_type))

INCLUDES_FOR_TYPE = {
    'Promise': set(['ScriptPromise.h']),
    'SerializedScriptValue': set(['bindings/v8/SerializedScriptValue.h']),
}

def includes_for_type(idl_type):
    if idl_type in INCLUDES_FOR_TYPE:
        return INCLUDES_FOR_TYPE[idl_type]
    if skip_includes(idl_type):
        return set()
    if is_typed_array_type(idl_type):
        return set(['bindings/v8/custom/V8%sCustom.h' % idl_type])
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return includes_for_type(this_array_or_sequence_type)
    return set(['V8%s.h' % idl_type])


################################################################################
# V8 -> C++
################################################################################

V8_VALUE_TO_CPP_VALUE_BASIC = {
    'Date': 'toWebCoreDate({v8_value})',
    'DOMString': '{v8_value}',
    'boolean': '{v8_value}->BooleanValue()',
    'float': 'static_cast<float>({v8_value}->NumberValue())',
    'double': 'static_cast<double>({v8_value}->NumberValue())',
    'byte': 'toInt8({arguments})',
    'octet': 'toUInt8({arguments})',
    'short': 'toInt32({arguments})',
    'unsigned short': 'toUInt32({arguments})',
    'long': 'toInt32({arguments})',
    'unsigned long': 'toUInt32({arguments})',
    'long long': 'toInt64({arguments})',
    'unsigned long long': 'toUInt64({arguments})',
}
V8_VALUE_TO_CPP_VALUE_AND_INCLUDES = {
    # idl_type -> (cpp_expression_format, includes)
    'any': ('ScriptValue({v8_value}, {isolate})',
            set(['bindings/v8/ScriptValue.h'])),
    'Dictionary': ('Dictionary({v8_value}, {isolate})',
                   set(['bindings/v8/Dictionary.h'])),
    'DOMStringList': ('toDOMStringList({v8_value}, {isolate})', set()),
    'MediaQueryListListener': ('MediaQueryListListener::create({v8_value})',
                               set(['core/css/MediaQueryListListener.h'])),
    'NodeFilter': ('toNodeFilter({v8_value}, {isolate})', set()),
    'Promise': ('ScriptPromise({v8_value})',
                set(['bindings/v8/ScriptPromise.h'])),
    'SerializedScriptValue': (
        'SerializedScriptValue::create({v8_value}, {isolate})',
        set(['bindings/v8/SerializedScriptValue.h'])),
}


def v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, includes, isolate):
    this_array_or_sequence_type = array_or_sequence_type(idl_type)
    if this_array_or_sequence_type:
        return v8_value_to_cpp_value_array_or_sequence(this_array_or_sequence_type, v8_value, includes, isolate)

    if is_callback_function_type(idl_type):
        idl_type = 'any'
    if is_enum_type(idl_type):
        idl_type = 'DOMString'

    if 'EnforceRange' in extended_attributes:
        arguments = ', '.join([v8_value, 'EnforceRange', 'ok'])
    else:  # NormalConversion
        arguments = v8_value

    if idl_type in V8_VALUE_TO_CPP_VALUE_BASIC:
        cpp_expression_format = V8_VALUE_TO_CPP_VALUE_BASIC[idl_type]
    elif idl_type in V8_VALUE_TO_CPP_VALUE_AND_INCLUDES:
        cpp_expression_format, new_includes = V8_VALUE_TO_CPP_VALUE_AND_INCLUDES[idl_type]
        includes.update(new_includes)
    else:
        cpp_expression_format = (
    'V8{idl_type}::HasInstance({v8_value}, {isolate}, worldType({isolate})) ? '
    'V8{idl_type}::toNative(v8::Handle<v8::Object>::Cast({v8_value})) : 0')
        includes.update(includes_for_type(idl_type))
        includes.add('V8%s.h' % idl_type)

    return cpp_expression_format.format(arguments=arguments, idl_type=idl_type, isolate=isolate, v8_value=v8_value)


def v8_value_to_cpp_value_array_or_sequence(this_array_or_sequence_type, v8_value, includes, isolate):
    if is_interface_type(this_array_or_sequence_type):
        this_cpp_type = None
        expression_format = '(toRefPtrNativeArray<{array_or_sequence_type}, V8{array_or_sequence_type}>({v8_value}, {isolate}))'
        includes.add('V8%s.h' % this_array_or_sequence_type)
    else:
        this_cpp_type = cpp_type(this_array_or_sequence_type)
        expression_format = 'toNativeArray<{cpp_type}>({v8_value}, {isolate})'
    expression = expression_format.format(array_or_sequence_type=this_array_or_sequence_type, cpp_type=this_cpp_type, isolate=isolate, v8_value=v8_value)
    return expression


def v8_value_to_local_cpp_value(idl_type, extended_attributes, v8_value, variable_name, includes, isolate):
    """Returns an expression that converts a V8 value to a C++ value and stores it as a local value."""
    this_cpp_type = cpp_type(idl_type, extended_attributes=extended_attributes, used_as_argument=True)

    if idl_type == 'DOMString' or is_enum_type(idl_type):
        format_string = 'V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID({cpp_type}, {variable_name}, {cpp_value})'
    elif 'EnforceRange' in extended_attributes:
        format_string = 'V8TRYCATCH_WITH_TYPECHECK_VOID({cpp_type}, {variable_name}, {cpp_value}, {isolate})'
    else:
        format_string = 'V8TRYCATCH_VOID({cpp_type}, {variable_name}, {cpp_value})'

    cpp_value = v8_value_to_cpp_value(idl_type, extended_attributes, v8_value, includes, isolate)
    return format_string.format(cpp_type=this_cpp_type, cpp_value=cpp_value, isolate=isolate, variable_name=variable_name)


################################################################################
# C++ -> V8
################################################################################


def preprocess_type_and_value(idl_type, cpp_value, extended_attributes):
    """Returns type and value, with preliminary type conversions applied."""
    if is_enum_type(idl_type):
        # Enumerations are internally DOMStrings
        idl_type = 'DOMString'
    if idl_type in ['Promise', 'any'] or is_callback_function_type(idl_type):
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


def v8_conversion_type(idl_type, extended_attributes, includes):
    """Returns V8 conversion type, adding any additional includes.

    The V8 conversion type is used to select the C++ -> V8 conversion function
    or v8SetReturnValue* function; it can be an idl_type, a cpp_type, or a
    separate name for the type of conversion (e.g., 'DOMWrapper').
    """
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
            includes.update(includes_for_type(this_array_or_sequence_type))
        return 'array'

    if idl_type == 'EventHandler':
        includes.add('bindings/v8/V8AbstractEventListener.h')
        return 'EventHandler'

    includes.update(includes_for_type(idl_type))
    if idl_type == 'SerializedScriptValue':
        return 'SerializedScriptValue'

    # Pointer type
    includes.add('wtf/GetPtr.h')  # FIXME: Is this necessary?
    includes.add('wtf/RefPtr.h')
    return 'DOMWrapper'


V8_SET_RETURN_VALUE = {
    'boolean': 'v8SetReturnValueBool({callback_info}, {cpp_value})',
    'int': 'v8SetReturnValueInt({callback_info}, {cpp_value})',
    'unsigned': 'v8SetReturnValueUnsigned({callback_info}, {cpp_value})',
    'DOMString': 'v8SetReturnValueString({callback_info}, {cpp_value}, {isolate})',
    # [TreatNullReturnValueAs]
    'StringOrNull': 'v8SetReturnValueStringOrNull({callback_info}, {cpp_value}, {isolate})',
    'StringOrUndefined': 'v8SetReturnValueStringOrUndefined({callback_info}, {cpp_value}, {isolate})',
    'void': '',
    # No special v8SetReturnValue* function (set value directly)
    'float': 'v8SetReturnValue({callback_info}, {cpp_value})',
    'double': 'v8SetReturnValue({callback_info}, {cpp_value})',
    # No special v8SetReturnValue* function, but instead convert value to V8
    # and then use general v8SetReturnValue.
    'array': 'v8SetReturnValue({callback_info}, {cpp_value})',
    'Date': 'v8SetReturnValue({callback_info}, {cpp_value})',
    'EventHandler': 'v8SetReturnValue({callback_info}, {cpp_value})',
    'ScriptValue': 'v8SetReturnValue({callback_info}, {cpp_value})',
    'SerializedScriptValue': 'v8SetReturnValue({callback_info}, {cpp_value})',
    # DOMWrapper
    'DOMWrapperFast': 'v8SetReturnValueFast({callback_info}, {cpp_value}, {script_wrappable})',
    'DOMWrapperDefault': 'v8SetReturnValue({callback_info}, {cpp_value}, {creation_context})',
}


def v8_set_return_value(idl_type, cpp_value, includes, callback_info, isolate, creation_context='', extended_attributes=None, script_wrappable=''):
    """Returns a statement that converts a C++ value to a V8 value and sets it as a return value."""
    def dom_wrapper_conversion_type():
        if not script_wrappable:
            return 'DOMWrapperDefault'
        return 'DOMWrapperFast'

    idl_type, cpp_value = preprocess_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes, includes)
    # SetReturn-specific overrides
    if this_v8_conversion_type in ['Date', 'EventHandler', 'ScriptValue', 'SerializedScriptValue', 'array']:
        # Convert value to V8 and then use general v8SetReturnValue
        cpp_value = cpp_value_to_v8_value(idl_type, cpp_value, includes, isolate, callback_info=callback_info, extended_attributes=extended_attributes)
    if this_v8_conversion_type == 'DOMWrapper':
        this_v8_conversion_type = dom_wrapper_conversion_type()

    format_string = V8_SET_RETURN_VALUE[this_v8_conversion_type]
    statement = format_string.format(callback_info=callback_info, cpp_value=cpp_value, creation_context=creation_context, isolate=isolate, script_wrappable=script_wrappable)
    return statement


CPP_VALUE_TO_V8_VALUE = {
    'Date': 'v8DateOrNull({cpp_value}, {isolate})',
    'DOMString': 'v8String({cpp_value}, {isolate})',
    'ScriptValue': '{cpp_value}.v8Value()',
    'SerializedScriptValue': '{cpp_value} ? {cpp_value}->deserialize() : v8::Handle<v8::Value>(v8::Null({isolate}))',
    'boolean': 'v8Boolean({cpp_value}, {isolate})',
    'int': 'v8::Integer::New({cpp_value}, {isolate})',
    'unsigned': 'v8::Integer::NewFromUnsigned({cpp_value}, {isolate})',
    'float': 'v8::Number::New({cpp_value})',
    'double': 'v8::Number::New({cpp_value})',
    'void': 'v8Undefined()',
    # Special cases
    'EventHandler': '{cpp_value} ? v8::Handle<v8::Value>(V8AbstractEventListener::cast({cpp_value})->getListenerObject(imp->executionContext())) : v8::Handle<v8::Value>(v8::Null({isolate}))',
    # General
    'array': 'v8Array({cpp_value}, {isolate})',
    'default': 'toV8({cpp_value}, {creation_context}, {isolate})',
}


def cpp_value_to_v8_value(idl_type, cpp_value, includes, isolate, callback_info='', creation_context='', extended_attributes=None):
    """Returns an expression that converts a C++ value to a V8 value."""
    idl_type, cpp_value = preprocess_type_and_value(idl_type, cpp_value, extended_attributes)
    this_v8_conversion_type = v8_conversion_type(idl_type, extended_attributes, includes)
    format_string = CPP_VALUE_TO_V8_VALUE[this_v8_conversion_type]
    statement = format_string.format(callback_info=callback_info, cpp_value=cpp_value, creation_context=creation_context, isolate=isolate)
    return statement
