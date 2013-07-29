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

import idl_definitions

# jinja2 is in chromium's third_party directory.
module_path, module_name = os.path.split(__file__)
third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir)
sys.path.append(third_party)
import jinja2


def apply_template(path_to_template, contents):
    dirname, basename = os.path.split(path_to_template)
    jinja_env = jinja2.Environment(trim_blocks=True, loader=jinja2.FileSystemLoader([dirname]))
    template = jinja_env.get_template(basename)
    return template.render(contents)


def v8_class_name(interface):
    return 'V8' + interface.name


def cpp_type(data_type):
    """Returns the C++ type corresponding to the IDL type."""
    if data_type == 'boolean':
        return 'bool'
    raise Exception('Not supported')


def callback_argument_declaration(operation):
    arguments = ['%s %s' % (cpp_type(argument.data_type), argument.name) for argument in operation.arguments]
    return ', '.join(arguments)


class CodeGeneratorV8:
    def __init__(self, definitions, interface_name, output_directory, idl_directories, verbose=False):
        self.idl_definitions = definitions
        self.interface_name = interface_name
        self.idl_directories = idl_directories
        self.output_directory = output_directory
        self.verbose = verbose
        self.interface = None

    def cpp_class_header_filename(self):
        """Returns relative path from bindings/ of webcore header of the interface"""
        # FIXME: parser will prepare posix form relative path from Source/bindings in IdlInterface.rel_path_posix
        idl_filename = self.idl_definitions.file_name
        idl_rel_path_local = os.path.relpath(idl_filename)
        idl_rel_path_posix = idl_rel_path_local.replace(os.path.sep, posixpath.sep)

        idl_dir_posix = posixpath.join('bindings', posixpath.dirname(idl_rel_path_posix))
        return posixpath.join(idl_dir_posix, self.interface.name + '.h')

    def write_dummy_header_and_cpp(self):
        target_interface_name = self.interface_name
        header_basename = 'V8%s.h' % target_interface_name
        cpp_basename = 'V8%s.cpp' % target_interface_name
        header_fullname = os.path.join(self.output_directory, header_basename)
        cpp_fullname = os.path.join(self.output_directory, cpp_basename)
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
        if self.interface_name not in self.idl_definitions.interfaces:
            raise Exception('%s not in IDL definitions' % self.interface_name)
        self.interface = self.idl_definitions.interfaces[self.interface_name]
        header_basename = v8_class_name(self.interface) + '.h'
        cpp_basename = v8_class_name(self.interface) + '.cpp'
        if self.interface.is_callback:
            template_contents = self.generate_callback_interface()
            header_file_text = apply_template('templates/callback.h', template_contents)
            cpp_file_text = apply_template('templates/callback.cpp', template_contents)
        else:
            # FIXME: Implement.
            header_file_text = ""
            cpp_file_text = ""
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

    def generate_callback_interface(self):
        cpp_includes = set([
            'core/dom/ScriptExecutionContext.h',
            'bindings/v8/V8Binding.h',
            'bindings/v8/V8Callback.h',
            'wtf/Assertions.h',
        ])
        header_includes = set([
            'bindings/v8/ActiveDOMCallback.h',
            'bindings/v8/DOMWrapperWorld.h',
            'bindings/v8/ScopedPersistent.h',
        ])
        header_includes.add(self.cpp_class_header_filename())

        def operation_to_method(operation):
            method = {}
            if 'Custom' not in operation.extended_attributes:
                if operation.data_type != 'boolean':
                    raise Exception("We don't yet support callbacks that return non-boolean values.")
                if len(operation.arguments):
                    raise Exception('Not supported')
                method = {
                    'return_type': cpp_type(operation.data_type),
                    'name': operation.name,
                    'arguments': [],
                    'argument_declaration': callback_argument_declaration(operation),
                    'custom': None,
                }
            return method

        methods = [operation_to_method(operation) for operation in self.interface.operations]
        template_contents = {
            'cpp_class_name': self.interface.name,
            'v8_class_name': v8_class_name(self.interface),
            'cpp_includes': sorted(list(cpp_includes)),
            'header_includes': sorted(list(header_includes)),
            'methods': methods,
        }
        return template_contents
