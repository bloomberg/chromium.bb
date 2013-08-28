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

"""Generate Blink V8 bindings (.h and .cpp files).

Input: An object of class IdlDefinitions, containing an IDL interface X
Output: V8X.h and V8X.cpp
"""

import os
import posixpath
import re
import sys

# jinja2 is in chromium's third_party directory.
module_path, module_name = os.path.split(__file__)
third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir)
sys.path.append(third_party)
import jinja2

templates_dir = os.path.join(module_path, os.pardir, 'templates')

ACRONYMS = ['CSS', 'HTML', 'IME', 'JS', 'SVG', 'URL', 'XML', 'XSLT']
CALLBACK_INTERFACE_CPP_INCLUDES = set([
    'core/dom/ScriptExecutionContext.h',
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8Callback.h',
    'wtf/Assertions.h',
])
CALLBACK_INTERFACE_H_INCLUDES = set([
    'bindings/v8/ActiveDOMCallback.h',
    'bindings/v8/DOMWrapperWorld.h',
    'bindings/v8/ScopedPersistent.h',
])
INTERFACE_CPP_INCLUDES = set([
    'RuntimeEnabledFeatures.h',
    'bindings/v8/ScriptController.h',
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMConfiguration.h',  # FIXME: necessary?
    'bindings/v8/V8DOMWrapper.h',  # FIXME: necessary?
    'core/dom/ContextFeatures.h',
    'core/dom/Document.h',
    'core/page/Frame.h',
    'core/platform/chromium/TraceEvent.h',
    'wtf/UnusedParam.h',
])
INTERFACE_H_INCLUDES = set([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8DOMWrapper.h',  # FIXME: necessary?
    'bindings/v8/WrapperTypeInfo.h',  # FIXME: necessary?
])
CPP_TYPE_SPECIAL_CONVERSION_RULES = {
    'float': 'float',
    'double': 'double',
    'long long': 'long long',
    'unsigned long long': 'unsigned long long',
    'long': 'int',
    'short': 'int',
    'byte': 'int',
    'boolean': 'bool',
    'DOMString': 'const String&',
}
CPP_UNSIGNED_TYPES = set([
    'octet',
    'unsigned int',
    'unsigned long',
    'unsigned short',
])
PRIMITIVE_TYPES = set([
    'boolean',
    'void',
    'Date',
    'byte',
    'octet',
    'short',
    'long',
    'long long',
    'unsigned short',
    'unsigned long',
    'unsigned long long',
    'float',
    'double',
])
V8_SET_RETURN_VALUE_DICT = {
    'unsigned': 'v8SetReturnValueUnsigned({callback_info}, {cpp_value});',
}


def apply_template(basename, contents):
    jinja_env = jinja2.Environment(trim_blocks=True, loader=jinja2.FileSystemLoader(templates_dir))
    template = jinja_env.get_template(basename)
    return template.render(contents)


def cpp_value_to_v8_value(data_type, cpp_value, isolate, creation_context=''):
    """Return an expression converting a C++ value to a V8 value."""
    if data_type == 'boolean':
        return 'v8Boolean(%s, %s)' % (cpp_value, isolate)
    if data_type in ['long long', 'unsigned long long', 'DOMTimeStamp']:
        # long long and unsigned long long are not representable in ECMAScript.
        return 'v8::Number::New(static_cast<double>(%s))' % cpp_value
    if primitive_type(data_type):
        if data_type not in ['float', 'double']:
            raise Exception('unexpected data_type %s' % data_type)
        return 'v8::Number::New(%s)' % cpp_value
    if data_type == 'DOMString':
        return 'v8String(%s, %s)' % (cpp_value, isolate)
    if array_or_sequence_type(data_type):
        return 'v8Array(%s, %s)' % (cpp_value, isolate)
    return 'toV8(%s, %s, %s)' % (cpp_value, creation_context, isolate)


def v8_set_return_value(data_type, cpp_value, callback_info=''):
    """Return an statement converting a C++ value to a V8 value and setting it as a return value."""
    this_cpp_type = cpp_type(data_type)
    if this_cpp_type in V8_SET_RETURN_VALUE_DICT:
        expression_format_string = V8_SET_RETURN_VALUE_DICT[this_cpp_type]
    else:
        raise Exception('unexpected data_type %s' % data_type)
    return expression_format_string.format(callback_info=callback_info, cpp_value=cpp_value)


def generate_conditional_string(interface_or_attribute_or_operation):
    if 'Conditional' not in interface_or_attribute_or_operation.extended_attributes:
        return ''
    conditional = interface_or_attribute_or_operation.extended_attributes['Conditional']
    for operator in ['&', '|']:
        if operator in conditional:
            conditions = set(conditional.split(operator))
            operator_separator = ' %s%s ' % (operator, operator)
            return operator_separator.join(['ENABLE(%s)' % expression for expression in sorted(conditions)])
    return 'ENABLE(%s)' % conditional


def generate_constants(interface):
    return [generate_constant(constant) for constant in interface.constants]


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


def runtime_enable_function_name(definition_or_member):
    """Return the name of the RuntimeEnabledFeatures function.

    The returned function checks if a method/attribute is enabled.
    Given extended attribute EnabledAtRuntime=FeatureName, return:
        RuntimeEnabledFeatures::{featureName}Enabled
    Note that the initial character or acronym is uncapitalized.
    """
    feature_name = definition_or_member.extended_attributes['EnabledAtRuntime']
    return 'RuntimeEnabledFeatures::%sEnabled' % uncapitalize(feature_name)


def uncapitalize(name):
    """Uncapitalize first letter or initial acronym (* with some exceptions).

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    Used in method names; exceptions differ from capitalize().
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            name.replace(acronym, acronym.lower())
            return name
    return name[0].lower() + name[1:]


def includes_for_type(data_type):
    if primitive_type(data_type) or data_type == 'DOMString':
        return set()
    if array_or_sequence_type(data_type):
        return includes_for_type(array_or_sequence_type(data_type))
    return set(['V8%s.h' % data_type])


def includes_for_cpp_class(class_name, relative_dir_posix):
    return set([posixpath.join(relative_dir_posix, class_name + '.h')])


def includes_for_operation(operation):
    includes = includes_for_type(operation.data_type)
    for parameter in operation.arguments:
        includes |= includes_for_type(parameter.data_type)
    return includes


def primitive_type(data_type):
    return data_type in PRIMITIVE_TYPES


def sequence_type(data_type):
    matched = re.match(r'sequence<([\w\d_\s]+)>', data_type)
    if not matched:
        return None
    return matched.group(1)


def array_type(data_type):
    matched = re.match(r'([\w\d_\s]+)\[\]', data_type)
    if not matched:
        return None
    return matched.group(1)


def array_or_sequence_type(data_type):
    return array_type(data_type) or sequence_type(data_type)


def cpp_type(idl_type, pointer_type=None):
    """Returns the C++ type corresponding to the IDL type.

    If unidentified, fall back to a pointer.

    Args:
        idl_type: IDL type
        pointer_type: If specified, return 'pointer_type<idl_type>'
            (e.g. RefPtr<Foo>, PassRefPtr<Foo>)
            else defaults to returning raw pointer form (e.g. 'Foo*').
    """
    if idl_type in CPP_TYPE_SPECIAL_CONVERSION_RULES:
        return CPP_TYPE_SPECIAL_CONVERSION_RULES[idl_type]
    if idl_type in CPP_UNSIGNED_TYPES:
        return 'unsigned'
    if array_or_sequence_type(idl_type):
        return 'const Vector<{cpp_type} >&'.format(cpp_type=cpp_type(array_or_sequence_type(idl_type), 'RefPtr'))
    if pointer_type:
        return '{pointer_type}<{idl_type}>'.format(pointer_type=pointer_type, idl_type=idl_type)
    return idl_type + '*'  # raw pointer


def v8_type(data_type):
    return 'V8' + data_type


def cpp_method_name(attribute_or_operation):
    return attribute_or_operation.extended_attributes.get('ImplementedAs', attribute_or_operation.name)


def cpp_class_name(interface):
    return interface.extended_attributes.get('ImplementedAs', interface.name)


def v8_class_name(interface):
    return v8_type(interface.name)


class CodeGeneratorV8:
    def __init__(self, definitions, interface_name, output_directory, relative_dir_posix, idl_directories, verbose=False):
        self.idl_definitions = definitions
        self.interface_name = interface_name
        self.idl_directories = idl_directories
        self.output_directory = output_directory
        self.relative_dir_posix = relative_dir_posix
        self.verbose = verbose
        self.interface = None
        self.header_includes = set()
        self.cpp_includes = set()
        if definitions:  # FIXME: remove check when remove write_dummy_header_and_cpp
            try:
                self.interface = definitions.interfaces[interface_name]
            except KeyError:
                raise Exception('%s not in IDL definitions' % interface_name)

    def generate_cpp_to_v8_conversion(self, data_type, cpp_value, format_string, isolate, creation_context=''):
        """Returns a statement that converts a C++ value to a V8 value.

        Also add necessary includes to self.cpp_includes.
        """
        self.cpp_includes |= includes_for_type(data_type)
        v8_value = cpp_value_to_v8_value(data_type, cpp_value, isolate, creation_context)
        return format_string % v8_value

    def write_dummy_header_and_cpp(self):
        # FIXME: fix GYP so these files aren't needed and remove this method
        target_interface_name = self.interface_name
        header_basename = 'V8%s.h' % target_interface_name
        cpp_basename = 'V8%s.cpp' % target_interface_name
        contents = """/*
    This file is generated just to tell build scripts that {header_basename} and
    {cpp_basename} are created for {target_interface_name}.idl, and thus
    prevent the build scripts from trying to generate {header_basename} and
    {cpp_basename} at every build. This file must not be tried to compile.
*/
""".format(**locals())
        self.write_header_code(header_basename, contents)
        self.write_cpp_code(cpp_basename, contents)

    def write_header_and_cpp(self):
        header_basename = v8_class_name(self.interface) + '.h'
        cpp_basename = v8_class_name(self.interface) + '.cpp'
        if self.interface.is_callback:
            header_template = 'callback_interface.h'
            cpp_template = 'callback_interface.cpp'
            template_contents = self.generate_callback_interface()
        else:
            header_template = 'interface.h'
            cpp_template = 'interface.cpp'
            template_contents = self.generate_interface()
        template_contents['conditional_string'] = generate_conditional_string(self.interface)
        header_file_text = apply_template(header_template, template_contents)
        cpp_file_text = apply_template(cpp_template, template_contents)
        self.write_header_code(header_basename, header_file_text)
        self.write_cpp_code(cpp_basename, cpp_file_text)

    def write_header_code(self, header_basename, header_file_text):
        header_filename = os.path.join(self.output_directory, header_basename)
        with open(header_filename, 'w') as header_file:
            header_file.write(header_file_text)

    def write_cpp_code(self, cpp_basename, cpp_file_text):
        cpp_filename = os.path.join(self.output_directory, cpp_basename)
        with open(cpp_filename, 'w') as cpp_file:
            cpp_file.write(cpp_file_text)

    def generate_attribute(self, attribute):
        data_type = attribute.data_type
        # FIXME: need to check should_keep_attribute_alive, but for now
        # sufficient to check if primitive.
        should_keep_attribute_alive = not primitive_type(data_type)
        if should_keep_attribute_alive:
            return_v8_value_statement = None  # unused
            self.cpp_includes |= includes_for_type(data_type)
            self.cpp_includes.add('bindings/v8/V8HiddenPropertyName.h')
        else:
            cpp_value = 'imp->%s()' % uncapitalize(attribute.name)
            return_v8_value_statement = v8_set_return_value(data_type, cpp_value, callback_info='info')
        return {
            'name': attribute.name,
            'conditional_string': generate_conditional_string(attribute),
            'cpp_method_name': cpp_method_name(attribute),
            'cpp_type': cpp_type(data_type, pointer_type='RefPtr'),
            'should_keep_attribute_alive': should_keep_attribute_alive,
            'return_v8_value_statement': return_v8_value_statement,
            'v8_type': v8_type(data_type),
        }

    def generate_interface(self):
        interface = self.interface
        self.header_includes = INTERFACE_H_INCLUDES
        self.header_includes |= includes_for_cpp_class(cpp_class_name(self.interface), self.relative_dir_posix)
        self.cpp_includes = INTERFACE_CPP_INCLUDES

        template_contents = {
            'interface_name': interface.name,
            'cpp_class_name': cpp_class_name(interface),
            'v8_class_name': v8_class_name(interface),
            'attributes': [self.generate_attribute(attribute) for attribute in interface.attributes],
            # Size 0 constant array is not allowed in VC++
            'number_of_attributes': 'WTF_ARRAY_LENGTH(%sAttributes)' % v8_class_name(interface) if interface.attributes else '0',
            'attribute_templates': v8_class_name(interface) + 'Attributes' if interface.attributes else '0',
            'constants': generate_constants(interface),
            'do_not_check_constants': 'DoNotCheckConstants' in interface.extended_attributes,
        }
        # Add includes afterwards, as they are modified by generate_attribute etc.
        template_contents['header_includes'] = sorted(list(self.header_includes))
        template_contents['cpp_includes'] = sorted(list(self.cpp_includes))
        return template_contents

    def generate_callback_interface(self):
        self.header_includes = CALLBACK_INTERFACE_H_INCLUDES
        self.header_includes |= includes_for_cpp_class(cpp_class_name(self.interface), self.relative_dir_posix)
        self.cpp_includes = CALLBACK_INTERFACE_CPP_INCLUDES

        def generate_argument(argument):
            receiver = 'v8::Handle<v8::Value> %sHandle = %%s;' % argument.name
            cpp_to_v8_conversion = self.generate_cpp_to_v8_conversion(argument.data_type, argument.name, receiver, 'isolate', creation_context='v8::Handle<v8::Object>()')
            return {
                'name': argument.name,
                'cpp_to_v8_conversion': cpp_to_v8_conversion,
            }

        def generate_method(operation):
            def argument_declaration(argument):
                return '%s %s' % (cpp_type(argument.data_type), argument.name)

            arguments = []
            custom = 'Custom' in operation.extended_attributes
            if not custom:
                self.cpp_includes |= includes_for_operation(operation)
                if operation.data_type != 'boolean':
                    raise Exception("We don't yet support callbacks that return non-boolean values.")
                arguments = [generate_argument(argument) for argument in operation.arguments]
            method = {
                'return_cpp_type': cpp_type(operation.data_type, 'RefPtr'),
                'name': operation.name,
                'arguments': arguments,
                'argument_declaration': ', '.join([argument_declaration(argument) for argument in operation.arguments]),
                'handles': ', '.join(['%sHandle' % argument.name for argument in operation.arguments]),
                'custom': custom,
            }
            return method

        methods = [generate_method(operation) for operation in self.interface.operations]
        template_contents = {
            'cpp_class_name': self.interface.name,
            'v8_class_name': v8_class_name(self.interface),
            'cpp_includes': sorted(list(self.cpp_includes)),
            'header_includes': sorted(list(self.header_includes)),
            'methods': methods,
        }
        return template_contents
