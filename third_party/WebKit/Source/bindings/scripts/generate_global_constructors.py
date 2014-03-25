#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates interface properties on global objects.

Concretely these are implemented as "constructor attributes", meaning
"attributes whose name ends with Constructor" (special-cased by code generator),
hence "global constructors" for short.

For reference on global objects, see:
http://www.chromium.org/blink/webidl/blink-idl-extended-attributes#TOC-GlobalContext-i-

Design document: http://www.chromium.org/developers/design-documents/idl-build
"""

import optparse
import os
import re
import sys

from utilities import get_file_contents, write_file, get_interface_extended_attributes_from_idl, is_callback_interface_from_idl

global_objects = {}


HEADER_FORMAT = """
// Stub header file for {{idl_basename}}
// Required because the IDL compiler assumes that a corresponding header file
// exists for each IDL file.
"""

def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option('--write-file-only-if-changed', type='int', help='if true, do not write an output file if it would be identical to the existing one, which avoids unnecessary rebuilds in ninja')

    options, args = parser.parse_args()

    if options.idl_files_list is None:
        parser.error('Must specify a file listing IDL files using --idl-files-list.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether output files are only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)

    return options, args


def record_global_constructors(idl_filename):
    interface_name, _ = os.path.splitext(os.path.basename(idl_filename))
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)

    # An interface property is produced for every non-callback interface
    # that does not have [NoInterfaceObject].
    # Callback interfaces with constants also have interface properties,
    # but there are none of these in Blink.
    # http://heycam.github.io/webidl/#es-interfaces
    if (is_callback_interface_from_idl(idl_file_contents) or
        'NoInterfaceObject' in extended_attributes):
        return

    global_contexts = extended_attributes.get('GlobalContext', 'Window').split('&')
    new_constructors_list = generate_global_constructors_list(interface_name, extended_attributes)
    for interface_name in global_contexts:
        global_objects[interface_name]['constructors'].extend(new_constructors_list)


def generate_global_constructors_list(interface_name, extended_attributes):
    extended_attributes_list = [
            name + '=' + extended_attributes[name]
            for name in 'Conditional', 'PerContextEnabled', 'RuntimeEnabled'
            if name in extended_attributes]
    if extended_attributes_list:
        extended_string = '[%s] ' % ', '.join(extended_attributes_list)
    else:
        extended_string = ''

    attribute_string = 'attribute {interface_name}Constructor {interface_name}'.format(interface_name=interface_name)
    attributes_list = [extended_string + attribute_string]

    # In addition to the usual interface property, for every [NamedConstructor]
    # extended attribute on an interface, a corresponding property MUST exist
    # on the ECMAScript global object.
    # http://heycam.github.io/webidl/#NamedConstructor
    if 'NamedConstructor' in extended_attributes:
        named_constructor = extended_attributes['NamedConstructor']
        # Extract function name, namely everything before opening '('
        constructor_name = re.sub(r'\(.*', '', named_constructor)
        # Note the reduplicated 'ConstructorConstructor'
        # FIXME: rename to NamedConstructor
        attribute_string = 'attribute %sConstructorConstructor %s' % (interface_name, constructor_name)
        attributes_list.append(extended_string + attribute_string)

    return attributes_list


def write_global_constructors_partial_interface(interface_name, idl_filename, constructor_attributes_list, only_if_changed):
    # FIXME: replace this with a simple Jinja template
    lines = (['partial interface %s {\n' % interface_name] +
             ['    %s;\n' % constructor_attribute
              # FIXME: sort by interface name (not first by extended attributes)
              for constructor_attribute in sorted(constructor_attributes_list)] +
             ['};\n'])
    write_file(''.join(lines), idl_filename, only_if_changed)
    header_filename = os.path.splitext(idl_filename)[0] + '.h'
    idl_basename = os.path.basename(idl_filename)
    write_file(HEADER_FORMAT.format(idl_basename=idl_basename),
               header_filename, only_if_changed)


################################################################################

def main():
    options, args = parse_options()

    # Input IDL files are passed in a file, due to OS command line length
    # limits. This is generated at GYP time, which is ok b/c files are static.
    with open(options.idl_files_list) as idl_files_list:
        idl_files = [line.rstrip('\n') for line in idl_files_list]

    # Output IDL files (to generate) are passed at the command line, since
    # these are in the build directory, which is determined at build time, not
    # GYP time.
    # These are passed as pairs of GlobalObjectName, GlobalObject.idl
    interface_name_idl_filename = [(args[i], args[i + 1])
                                   for i in range(0, len(args), 2)]
    global_objects.update(
        (interface_name, {
            'idl_filename': idl_filename,
            'constructors': [],
        })
        for interface_name, idl_filename in interface_name_idl_filename)

    for idl_filename in idl_files:
        record_global_constructors(idl_filename)

    for interface_name, global_object in global_objects.iteritems():
        write_global_constructors_partial_interface(
            interface_name,
            global_object['idl_filename'],
            global_object['constructors'],
            options.write_file_only_if_changed)


if __name__ == '__main__':
    sys.exit(main())
