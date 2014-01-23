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

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771

Input: An object of class IdlDefinitions, containing an IDL interface X
Output: V8X.h and V8X.cpp
"""

import os
import posixpath
import re
import sys

# jinja2 is in chromium's third_party directory.
module_path, module_name = os.path.split(__file__)
third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir)
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(1, third_party)
import jinja2

templates_dir = os.path.join(module_path, os.pardir, os.pardir, 'templates')

import v8_callback_interface
from v8_globals import includes, interfaces
import v8_interface
import v8_types
from v8_utilities import capitalize, cpp_name, conditional_string, v8_class_name


def write_header_and_cpp(definitions, interface_name, interfaces_info, output_directory, idl_directories, verbose=False):
    try:
        interface = definitions.interfaces[interface_name]
    except KeyError:
        raise Exception('%s not in IDL definitions' % interface_name)

    # Store other interfaces for introspection
    interfaces.update(definitions.interfaces)

    # Set up Jinja
    if interface.is_callback:
        header_template_filename = 'callback_interface.h'
        cpp_template_filename = 'callback_interface.cpp'
        generate_contents = v8_callback_interface.generate_callback_interface
    else:
        header_template_filename = 'interface.h'
        cpp_template_filename = 'interface.cpp'
        generate_contents = v8_interface.generate_interface
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        'blink_capitalize': capitalize,
        'conditional': conditional_if_endif,
        'runtime_enabled': runtime_enabled_if,
        })
    header_template = jinja_env.get_template(header_template_filename)
    cpp_template = jinja_env.get_template(cpp_template_filename)

    # Set type info, both local and global
    interface_info = interfaces_info[interface_name]

    v8_types.set_callback_functions(definitions.callback_functions.keys())
    v8_types.set_enums((enum.name, enum.values)
                       for enum in definitions.enumerations.values())
    v8_types.set_ancestors(dict(
        (interface_name, interface_info['ancestors'])
        for interface_name, interface_info in interfaces_info.iteritems()
        if 'ancestors' in interface_info))
    v8_types.set_callback_interfaces(set(
        interface_name
        for interface_name, interface_info in interfaces_info.iteritems()
        if interface_info['is_callback_interface']))
    v8_types.set_implemented_as_interfaces(dict(
        (interface_name, interface_info['implemented_as'])
        for interface_name, interface_info in interfaces_info.iteritems()
        if 'implemented_as' in interface_info))

    # Generate contents (input parameters for Jinja)
    template_contents = generate_contents(interface)
    template_contents['header_includes'].add(interface_info['include_path'])
    template_contents['header_includes'] = sorted(template_contents['header_includes'])
    includes.update(interface_info.get('dependencies_include_paths', []))
    template_contents['cpp_includes'] = sorted(includes)

    # Render Jinja templates and write files
    def write_file(basename, file_text):
        filename = os.path.join(output_directory, basename)
        with open(filename, 'w') as output_file:
            output_file.write(file_text)

    header_basename = v8_class_name(interface) + '.h'
    header_file_text = header_template.render(template_contents)
    write_file(header_basename, header_file_text)

    cpp_basename = v8_class_name(interface) + '.cpp'
    cpp_file_text = cpp_template.render(template_contents)
    write_file(cpp_basename, cpp_file_text)


# [Conditional]
def conditional_if_endif(code, conditional_string):
    # Jinja2 filter to generate if/endif directive blocks
    if not conditional_string:
        return code
    return ('#if %s\n' % conditional_string +
            code +
            '#endif // %s\n' % conditional_string)


# [RuntimeEnabled]
def runtime_enabled_if(code, runtime_enabled_function_name):
    if not runtime_enabled_function_name:
        return code
    # Indent if statement to level of original code
    indent = re.match(' *', code).group(0)
    return ('%sif (%s())\n' % (indent, runtime_enabled_function_name) +
            '    %s' % code)
