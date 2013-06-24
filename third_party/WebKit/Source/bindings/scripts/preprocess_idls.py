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

import optparse
import os
import os.path
import re
import string


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--idl-files-list', help='file listing all IDLs')
    parser.add_option('--supplemental-dependency-file', help='output file')
    parser.add_option('--window-constructors-file', help='output file')
    parser.add_option('--workercontext-constructors-file', help='output file')
    parser.add_option('--write-file-only-if-changed', type='int')
    options, args = parser.parse_args()
    if options.supplemental_dependency_file is None:
        parser.error('Must specify an output file using --supplemental-dependency-file.')
    if options.window_constructors_file is None:
        parser.error('Must specify an output file using --window-constructors-file.')
    if options.workercontext_constructors_file is None:
        parser.error('Must specify an output file using --workercontext-constructors-file.')
    if options.idl_files_list is None:
        parser.error('Must specify the file listing all IDLs using --idl-files-list.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether file is only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)
    if args:
        parser.error('No arguments taken, but "%s" given.' % ' '.join(args))
    return options


def get_file_contents(idl_filename):
    with open(idl_filename) as idl_file:
        lines = idl_file.readlines()
    return ''.join(lines)


def get_partial_interface_name_from_idl(file_contents):
    match = re.search(r'partial\s+interface\s+(\w+)', file_contents)
    if match:
        return match.group(1)
    return None


# identifier-A implements identifier-B;
# http://www.w3.org/TR/WebIDL/#idl-implements-statements
def get_implemented_interfaces_from_idl(file_contents, interface_name):
    implemented_interfaces = []
    for match in re.finditer(r'^\s*(\w+)\s+implements\s+(\w+)\s*;', file_contents, re.MULTILINE):
        # identifier-A must be the current interface
        assert match.group(1) == interface_name, \
"Identifier on the left of the 'implements' statement should be %s in %s.idl, but found %s" % (interface_name, interface_name, match.group(1))
        implemented_interfaces.append(match.group(2))
    return implemented_interfaces

def is_callback_interface_from_idl(file_contents):
    match = re.search(r'callback\s+interface\s+\w+', file_contents)
    return match is not None


def get_interface_extended_attributes_from_idl(file_contents):
    extended_attributes = {}
    match = re.search(r'\[(.*)\]\s+(interface|exception)\s+(\w+)',
                      file_contents, flags=re.DOTALL)
    if match:
        parts = string.split(match.group(1), ',')
        for part in parts:
            # Match 'key = value'
            match = re.match(r'([^=\s]*)(?:\s*=\s*(.*))?', part.strip())
            key = match.group(1)
            value = match.group(2) or 'VALUE_IS_MISSING'
            if not key:
                continue
            extended_attributes[key] = value
    return extended_attributes


def generate_constructor_attribute_list(interface_name, extended_attributes):
    extended_attributes_list = []
    for attribute_name, attribute_value in extended_attributes.iteritems():
        if attribute_name not in ['Conditional', 'EnabledAtRuntime', 'EnabledPerContext']:
            continue
        extended_attribute = attribute_name
        if attribute_value != 'VALUE_IS_MISSING':
            extended_attribute += '=' + attribute_value
        extended_attributes_list.append(extended_attribute)
    if extended_attributes_list:
        extended_string = '[%s] ' % ', '.join(extended_attributes_list)
    else:
        extended_string = ''

    attribute_string = 'attribute %(interface_name)sConstructor %(interface_name)s' % {'interface_name': interface_name}
    attributes_list = [extended_string + attribute_string]

    # In addition to the regular property, for every [NamedConstructor]
    # extended attribute on an interface, a corresponding property MUST exist
    # on the ECMAScript global object.
    if 'NamedConstructor' in extended_attributes:
        named_constructor = extended_attributes['NamedConstructor']
        # Extract function name, namely everything before opening '('
        constructor_name = re.sub(r'\(.*', '', named_constructor)
        # Note the reduplicated 'ConstructorConstructor'
        attribute_string = 'attribute %sConstructorConstructor %s' % (interface_name, constructor_name)
        attributes_list.append(extended_string + attribute_string)

    return attributes_list


def generate_global_constructors_partial_interface(interface_name, destination_filename, constructor_attributes_list):
    with open(destination_filename, 'w') as destination_file:
        destination_file.write('partial interface %s {\n' % interface_name)
        for constructor_attribute in constructor_attributes_list:
            destination_file.write('    %s;\n' % constructor_attribute)
        destination_file.write('};\n')


def parse_idl_files(idl_files, window_constructors_filename, workercontext_constructors_filename):
    interface_name_to_idl_file = {}
    idl_file_to_interface_name = {}
    supplemental_dependencies = {}
    supplementals = {}
    window_constructor_attributes_list = []
    workercontext_constructor_attributes_list = []

    # Populate interface_name_to_idl_file first
    for idl_file_name in idl_files:
        full_path = os.path.realpath(idl_file_name)
        interface_name, _ = os.path.splitext(os.path.basename(idl_file_name))
        interface_name_to_idl_file[interface_name] = full_path

    for idl_file_name in idl_files:
        full_path = os.path.realpath(idl_file_name)
        idl_file_contents = get_file_contents(full_path)
        # Handle partial interfaces
        partial_interface_name = get_partial_interface_name_from_idl(idl_file_contents)
        if partial_interface_name:
            supplemental_dependencies[full_path] = [partial_interface_name]
            continue
        interface_name, _ = os.path.splitext(os.path.basename(idl_file_name))
        # Parse 'identifier-A implements identifier-B; statements
        implemented_interfaces = get_implemented_interfaces_from_idl(idl_file_contents, interface_name)
        for implemented_interface in implemented_interfaces:
            assert implemented_interface in interface_name_to_idl_file, \
"Could not find a the IDL file where the following implemented interface is defined: %s" % implemented_interface
            supplemental_dependencies.setdefault(interface_name_to_idl_file[implemented_interface], []).append(interface_name)
        # Handle [NoInterfaceObject]
        if not is_callback_interface_from_idl(idl_file_contents):
            extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)
            if 'NoInterfaceObject' not in extended_attributes:
                global_context = extended_attributes.get("GlobalContext", "WindowOnly")
                constructor_list = generate_constructor_attribute_list(interface_name, extended_attributes)
                if global_context != "WorkerOnly":
                    window_constructor_attributes_list.extend(constructor_list)
                if global_context != "WindowOnly":
                    workercontext_constructor_attributes_list.extend(constructor_list)
        idl_file_to_interface_name[full_path] = interface_name
        supplementals[full_path] = []

    # Generate Global constructors
    generate_global_constructors_partial_interface("Window", window_constructors_filename, window_constructor_attributes_list)
    if 'Window' in interface_name_to_idl_file:
        supplemental_dependencies[window_constructors_filename] = ['Window']
    generate_global_constructors_partial_interface("WorkerContext", workercontext_constructors_filename, workercontext_constructor_attributes_list)
    if 'WorkerContext' in interface_name_to_idl_file:
        supplemental_dependencies[workercontext_constructors_filename] = ['WorkerContext']

    # Resolve partial interfaces dependencies
    for idl_file, base_files in supplemental_dependencies.iteritems():
        for base_file in base_files:
            target_idl_file = interface_name_to_idl_file[base_file]
            supplementals[target_idl_file].append(idl_file)
            if idl_file in supplementals:
                # Should never occur. Might be needed in corner cases.
                del supplementals[idl_file]
    return supplementals


def write_dependency_file(filename, supplementals, only_if_changed=False):
    """Outputs the dependency file.

    The format of a supplemental dependency file:

    Window.idl P.idl Q.idl R.idl
    Document.idl S.idl
    Event.idl
    ...

    The above indicates that:
    Window.idl is supplemented by P.idl, Q.idl and R.idl,
    Document.idl is supplemented by S.idl, and
    Event.idl is supplemented by no IDLs.

    An IDL that supplements another IDL (e.g. P.idl) does not have its own
    lines in the dependency file.
    """
    new_lines = []
    for idl_file, supplemental_files in sorted(supplementals.iteritems()):
        new_lines.append('%s %s\n' % (idl_file, ' '.join(supplemental_files)))
    if only_if_changed and os.path.isfile(filename):
        with open(filename) as out_file:
            old_lines = out_file.readlines()
        if old_lines == new_lines:
            return
    with open(filename, 'w') as out_file:
        out_file.write(''.join(new_lines))


def main():
    options = parse_options()
    idl_files = []
    with open(options.idl_files_list) as idl_files_list_file:
        for line in idl_files_list_file:
            idl_files.append(string.rstrip(line, '\n'))
    resolved_supplementals = parse_idl_files(idl_files, options.window_constructors_file, options.workercontext_constructors_file)
    write_dependency_file(options.supplemental_dependency_file, resolved_supplementals, only_if_changed=options.write_file_only_if_changed)


if __name__ == '__main__':
    main()
