#!/usr/bin/python
#
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
import cPickle as pickle
import posixpath
import re
import string

module_path, _ = os.path.split(__file__)
source_path = os.path.normpath(os.path.join(module_path, os.pardir, os.pardir))

INHERITED_EXTENDED_ATTRIBUTES = set([
    'ActiveDOMObject',
    'DependentLifetime',
])


# interfaces_info is *exported* (in a pickle), and should only contain data
# about an interface that contains paths or is needed by *other* interfaces,
# i.e., file layout data (to abstract the compiler from file paths) or
# public data (to avoid having to read other interfaces unnecessarily).
# It should *not* contain full information about an interface (e.g., all
# extended attributes), as this would cause unnecessary rebuilds.
interfaces_info = {}

# Auxiliary variables (not visible to future build steps)
partial_interface_files = {}
parent_interfaces = {}
extended_attributes_by_interface = {}  # interface name -> extended attributes


class IdlBadFilenameError(Exception):
    """Raised if an IDL filename disagrees with the interface name in the file."""
    pass


class IdlInterfaceFileNotFoundError(Exception):
    """Raised if the IDL file implementing an interface cannot be found."""
    pass


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--event-names-file', help='output file')
    parser.add_option('--idl-files-list', help='file listing IDL files')
    parser.add_option('--interface-dependencies-file', help='output file')
    parser.add_option('--interfaces-info-file', help='output pickle file')
    parser.add_option('--window-constructors-file', help='output file')
    parser.add_option('--workerglobalscope-constructors-file', help='output file')
    parser.add_option('--sharedworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--dedicatedworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--serviceworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--write-file-only-if-changed', type='int', help='if true, do not write an output file if it would be identical to the existing one, which avoids unnecessary rebuilds in ninja')
    options, args = parser.parse_args()
    if options.event_names_file is None:
        parser.error('Must specify an output file using --event-names-file.')
    if options.interface_dependencies_file is None:
        parser.error('Must specify an output file using --interface-dependencies-file.')
    if options.interfaces_info_file is None:
        parser.error('Must specify an output file using --interfaces-info-file.')
    if options.window_constructors_file is None:
        parser.error('Must specify an output file using --window-constructors-file.')
    if options.workerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --workerglobalscope-constructors-file.')
    if options.sharedworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --sharedworkerglobalscope-constructors-file.')
    if options.dedicatedworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --dedicatedworkerglobalscope-constructors-file.')
    if options.serviceworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --serviceworkerglobalscope-constructors-file.')
    if options.idl_files_list is None:
        parser.error('Must specify a file listing IDL files using --idl-files-list.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether file is only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)
    if args:
        parser.error('No arguments taken, but "%s" given.' % ' '.join(args))
    return options


################################################################################
# Basic file reading/writing
################################################################################

def get_file_contents(idl_filename):
    with open(idl_filename) as idl_file:
        lines = idl_file.readlines()
    return ''.join(lines)


def write_file(new_lines, destination_filename, only_if_changed):
    if only_if_changed and os.path.isfile(destination_filename):
        with open(destination_filename) as destination_file:
            if destination_file.readlines() == new_lines:
                return
    with open(destination_filename, 'w') as destination_file:
        destination_file.write(''.join(new_lines))


def write_pickle_file(pickle_filename, data, only_if_changed):
    if only_if_changed and os.path.isfile(pickle_filename):
        with open(pickle_filename) as pickle_file:
            if pickle.load(pickle_file) == data:
                return
    with open(pickle_filename, 'w') as pickle_file:
        pickle.dump(data, pickle_file)


################################################################################
# IDL parsing
#
# We use regular expressions for parsing; this is incorrect (Web IDL is not a
# regular language), but simple and sufficient in practice.
# Leading and trailing context (e.g. following '{') used to avoid false matches.
################################################################################

def get_partial_interface_name_from_idl(file_contents):
    match = re.search(r'partial\s+interface\s+(\w+)\s*{', file_contents)
    return match and match.group(1)


# identifier-A implements identifier-B;
# http://www.w3.org/TR/WebIDL/#idl-implements-statements
def get_implemented_interfaces_from_idl(file_contents, interface_name):
    def get_implemented(left_identifier, right_identifier):
        # identifier-A must be the current interface
        if left_identifier != interface_name:
            raise IdlBadFilenameError("Identifier on the left of the 'implements' statement should be %s in %s.idl, but found %s" % (interface_name, interface_name, left_identifier))
        return right_identifier

    implements_re = (r'^\s*'
                     r'(\w+)\s+'
                     r'implements\s+'
                     r'(\w+)\s*'
                     r';')
    implements_matches = re.finditer(implements_re, file_contents, re.MULTILINE)
    implements_pairs = [(match.group(1), match.group(2))
                        for match in implements_matches]
    return [get_implemented(left, right) for left, right in implements_pairs]


def is_callback_interface_from_idl(file_contents):
    match = re.search(r'callback\s+interface\s+\w+\s*{', file_contents)
    return bool(match)


def get_parent_interface(file_contents):
    match = re.search(r'interface\s+'
                      r'\w+\s*'
                      r':\s*(\w+)\s*'
                      r'{',
                      file_contents)
    return match and match.group(1)


def get_interface_extended_attributes_from_idl(file_contents):
    match = re.search(r'\[(.*)\]\s+'
                      r'((callback|partial)\s+)?'
                      r'(interface|exception)\s+'
                      r'\w+\s*'
                      r'(:\s*\w+\s*)?'
                      r'{',
                      file_contents, flags=re.DOTALL)
    if not match:
        return {}
    # Strip comments
    # re.compile needed b/c Python 2.6 doesn't support flags in re.sub
    single_line_comment_re = re.compile(r'//.*$', flags=re.MULTILINE)
    block_comment_re = re.compile(r'/\*.*?\*/', flags=re.MULTILINE | re.DOTALL)
    extended_attributes_string = re.sub(single_line_comment_re, '', match.group(1))
    extended_attributes_string = re.sub(block_comment_re, '', extended_attributes_string)
    extended_attributes = {}
    # FIXME: this splitting is WRONG: it fails on ExtendedAttributeArgList like
    # 'NamedConstructor=Foo(a, b)'
    parts = [extended_attribute.strip()
             for extended_attribute in extended_attributes_string.split(',')
             # Discard empty parts, which may exist due to trailing comma
             if extended_attribute.strip()]
    for part in parts:
        name, _, value = map(string.strip, part.partition('='))
        extended_attributes[name] = value
    return extended_attributes


################################################################################
# Write files
################################################################################

def write_dependencies_file(dependencies_filename, only_if_changed):
    """Write the interface dependencies file.

    The format is as follows:

    Document.idl P.idl
    Event.idl
    Window.idl Q.idl R.idl S.idl
    ...

    The above indicates that:
    Document.idl depends on P.idl,
    Event.idl depends on no other IDL files, and
    Window.idl depends on Q.idl, R.idl, and S.idl.

    An IDL that is a dependency of another IDL (e.g. P.idl) does not have its
    own line in the dependency file.
    """
    # FIXME: remove text format once Perl gone (Python uses pickle)
    dependencies_list = sorted(
        (interface_info['full_path'], sorted(interface_info['dependencies_full_paths']))
        for interface_info in interfaces_info.values())
    lines = ['%s %s\n' % (idl_file, ' '.join(dependency_files))
             for idl_file, dependency_files in dependencies_list]
    write_file(lines, dependencies_filename, only_if_changed)


def write_event_names_file(destination_filename, only_if_changed):
    # Generate event names for all interfaces that inherit from Event,
    # including Event itself.
    event_names = set(
        interface_name
        for interface_name, interface_info in interfaces_info.iteritems()
        if (interface_name == 'Event' or
            ('ancestors' in interface_info and
             interface_info['ancestors'][-1] == 'Event')))

    def extended_attribute_string(name):
        value = extended_attributes[name]
        if name == 'RuntimeEnabled':
            value += 'Enabled'
        return name + '=' + value

    source_dir, _ = os.path.split(os.getcwd())
    lines = []
    lines.append('namespace="Event"\n')
    lines.append('\n')
    for filename, extended_attributes in sorted(
        (interface_info['full_path'],
         extended_attributes_by_interface[interface_name])
        for interface_name, interface_info in interfaces_info.iteritems()
        if interface_name in event_names):
        refined_filename, _ = os.path.splitext(os.path.relpath(filename, source_dir))
        refined_filename = refined_filename.replace(os.sep, posixpath.sep)
        extended_attributes_list = [
            extended_attribute_string(name)
            for name in 'Conditional', 'ImplementedAs', 'RuntimeEnabled'
            if name in extended_attributes]
        lines.append('%s %s\n' % (refined_filename, ', '.join(extended_attributes_list)))
    write_file(lines, destination_filename, only_if_changed)


def write_global_constructors_partial_interface(interface_name, destination_filename, constructor_attributes_list, only_if_changed):
    lines = (['partial interface %s {\n' % interface_name] +
             ['    %s;\n' % constructor_attribute
              for constructor_attribute in sorted(constructor_attributes_list)] +
             ['};\n'])
    write_file(lines, destination_filename, only_if_changed)


################################################################################
# Dependency resolution
################################################################################

def include_path(idl_filename, implemented_as=None):
    """Returns relative path to header file in POSIX format; used in includes.

    POSIX format is used for consistency of output, so reference tests are
    platform-independent.
    """
    relative_path_local = os.path.relpath(idl_filename, source_path)
    relative_dir_local = os.path.dirname(relative_path_local)
    relative_dir_posix = relative_dir_local.replace(os.path.sep, posixpath.sep)

    idl_file_basename, _ = os.path.splitext(os.path.basename(idl_filename))
    cpp_class_name = implemented_as or idl_file_basename

    return posixpath.join(relative_dir_posix, cpp_class_name + '.h')


def add_paths_to_partials_dict(partial_interface_name, full_path, this_include_path=None):
    paths_dict = partial_interface_files.setdefault(partial_interface_name,
                                                    {'full_paths': [],
                                                     'include_paths': []})
    paths_dict['full_paths'].append(full_path)
    if this_include_path:
        paths_dict['include_paths'].append(this_include_path)


def generate_dependencies(idl_filename):
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)

    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)
    implemented_as = extended_attributes.get('ImplementedAs')
    this_include_path = (
        include_path(idl_filename, implemented_as)
        # implemented interfaces with [LegacyImplementedInBaseClass] have no
        # header of their own, as they just use the header of the base class
        if 'LegacyImplementedInBaseClass' not in extended_attributes else None)

    # Handle partial interfaces
    partial_interface_name = get_partial_interface_name_from_idl(idl_file_contents)
    if partial_interface_name:
        add_paths_to_partials_dict(partial_interface_name, full_path, this_include_path)
        return partial_interface_name

    # If not a partial interface, the basename is the interface name
    interface_name, _ = os.path.splitext(os.path.basename(idl_filename))

    # Non-partial interfaces default to having bindings generated,
    # but are removed later if they are implemented by another interface
    interfaces_info[interface_name] = {
        'full_path': full_path,
        'implements_interfaces': get_implemented_interfaces_from_idl(idl_file_contents, interface_name),
        'is_callback_interface': is_callback_interface_from_idl(idl_file_contents),
    }
    if this_include_path:
        interfaces_info[interface_name]['include_path'] = this_include_path
    if implemented_as:
        interfaces_info[interface_name]['implemented_as'] = implemented_as

    return None


def remove_interfaces_implemented_somewhere():
    # Interfaces that are implemented by another interface do not have
    # their own bindings generated, as this would be redundant with the
    # actual implementation.
    implemented_somewhere = set().union(*[
        interface_info['implements_interfaces']
        for interface_info in interfaces_info.values()])
    for interface in implemented_somewhere:
        del interfaces_info[interface]


def generate_constructor_attribute_list(interface_name, extended_attributes):
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


def record_global_constructors_and_extended_attributes(idl_filename, global_constructors):
    interface_name, _ = os.path.splitext(os.path.basename(idl_filename))
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)

    # Record global constructors
    if not is_callback_interface_from_idl(idl_file_contents) and 'NoInterfaceObject' not in extended_attributes:
        global_contexts = extended_attributes.get('GlobalContext', 'Window').split('&')
        new_constructor_list = generate_constructor_attribute_list(interface_name, extended_attributes)
        for global_object in global_contexts:
            global_constructors[global_object].extend(new_constructor_list)

    # Record parents and extended attributes for generating event names
    parent = get_parent_interface(idl_file_contents)
    if parent:
        parent_interfaces[interface_name] = parent
    if parent or interface_name == 'Event':
        extended_attributes_by_interface[interface_name] = extended_attributes


def record_extended_attributes(idl_filename):
    interface_name, _ = os.path.splitext(os.path.basename(idl_filename))
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)
    extended_attributes_by_interface[interface_name] = extended_attributes


def generate_ancestors_and_inherited_extended_attributes(interface_name):
    interface_info = interfaces_info[interface_name]

    def generate_ancestors(interface_name):
        while interface_name in parent_interfaces:
            interface_name = parent_interfaces[interface_name]
            yield interface_name

    ancestors = list(generate_ancestors(interface_name))
    interface_info['ancestors'] = ancestors

    # Base interface (most distant ancestor) has no parent, and thus its
    # extended attributes may not yet be recorded.
    base_interface_name = ancestors[-1]
    if base_interface_name not in extended_attributes_by_interface:
        if base_interface_name not in interfaces_info:
            # Absent for support files
            return
        base_interface_info = interfaces_info[base_interface_name]
        base_interface_filename = base_interface_info['full_path']
        record_extended_attributes(base_interface_filename)

    inherited_extended_attributes = {}
    for ancestor in ancestors:
        ancestor_extended_attributes = extended_attributes_by_interface[ancestor]
        inherited_extended_attributes.update(dict(
            (key, value)
            for key, value in ancestor_extended_attributes.iteritems()
            if key in INHERITED_EXTENDED_ATTRIBUTES))
    if inherited_extended_attributes:
        interface_info['inherited_extended_attributes'] = inherited_extended_attributes


def parse_idl_files(idl_files, global_constructors_filenames):
    """Compute dependencies between IDL files, and return constructors on global objects.

    Primary effect is computing info about main interfaces, stored in global
    interfaces_info.
    The keys are the interfaces for which bindings are generated;
    this does not include interfaces implemented by another interface.

    Returns:
        global_constructors:
            dict of global objects -> list of constructors on that object
    """
    global_constructors = dict([
        (global_object, [])
        for global_object in global_constructors_filenames])

    # Generate dependencies, and (for main IDL files), record
    # global_constructors and extended_attributes_by_interface.
    for idl_filename in idl_files:
        # Test skips partial interfaces
        if not generate_dependencies(idl_filename):
            record_global_constructors_and_extended_attributes(idl_filename, global_constructors)

    for interface_name in parent_interfaces:
        generate_ancestors_and_inherited_extended_attributes(interface_name)

    # Add constructors on global objects to partial interfaces
    # These are all partial interfaces, but the files are dynamically generated,
    # so they need to be handled separately from static partial interfaces.
    for global_object, constructor_filename in global_constructors_filenames.iteritems():
        if global_object in interfaces_info:
            # No include path needed, as already included in the header file
            add_paths_to_partials_dict(global_object, constructor_filename)

    # An IDL file's dependencies are partial interface files that extend it,
    # and files for other interfaces that this interfaces implements.
    for interface_name, interface_info in interfaces_info.iteritems():
        partial_interfaces_full_paths, partial_interfaces_include_paths = (
                (partial_interface_files[interface_name]['full_paths'],
                 partial_interface_files[interface_name]['include_paths'])
                if interface_name in partial_interface_files else ([], []))

        implemented_interfaces = interface_info['implements_interfaces']
        try:
            implemented_interfaces_full_paths = [
                interfaces_info[interface]['full_path']
                for interface in implemented_interfaces]
            implemented_interfaces_include_paths = [
                interfaces_info[interface]['include_path']
                for interface in implemented_interfaces
                if 'include_path' in interfaces_info[interface]]
        except KeyError as key_name:
            raise IdlInterfaceFileNotFoundError('Could not find the IDL file where the following implemented interface is defined: %s' % key_name)

        interface_info['dependencies_full_paths'] = (
                partial_interfaces_full_paths +
                implemented_interfaces_full_paths)
        interface_info['dependencies_include_paths'] = (
                partial_interfaces_include_paths +
                implemented_interfaces_include_paths)
    remove_interfaces_implemented_somewhere()

    return global_constructors


################################################################################

def main():
    options = parse_options()
    with open(options.idl_files_list) as idl_files_list:
        idl_files = [line.rstrip('\n') for line in idl_files_list]
    only_if_changed = options.write_file_only_if_changed
    global_constructors_filenames = {
        'Window': options.window_constructors_file,
        'WorkerGlobalScope': options.workerglobalscope_constructors_file,
        'SharedWorkerGlobalScope': options.sharedworkerglobalscope_constructors_file,
        'DedicatedWorkerGlobalScope': options.dedicatedworkerglobalscope_constructors_file,
        'ServiceWorkerGlobalScope': options.serviceworkerglobalscope_constructors_file,
        }

    global_constructors = parse_idl_files(idl_files, global_constructors_filenames)

    write_dependencies_file(options.interface_dependencies_file, only_if_changed)
    write_pickle_file(options.interfaces_info_file, interfaces_info, only_if_changed)
    for interface_name, filename in global_constructors_filenames.iteritems():
        if interface_name in interfaces_info:
            write_global_constructors_partial_interface(interface_name, filename, global_constructors[interface_name], only_if_changed)
    write_event_names_file(options.event_names_file, only_if_changed)


if __name__ == '__main__':
    main()
