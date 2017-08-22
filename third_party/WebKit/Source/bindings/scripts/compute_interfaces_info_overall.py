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

"""Compute global interface information, including public information, dependencies, and inheritance.

Computed data is stored in a global variable, |interfaces_info|, and written as
output (concretely, exported as a pickle). This is then used by the IDL compiler
itself, so it does not need to compute global information itself, and so that
inter-IDL dependencies are clear, since they are all computed here.

The |interfaces_info| pickle is a *global* dependency: any changes cause a full
rebuild. This is to avoid having to compute which public data is visible by
which IDL files on a file-by-file basis, which is very complex for little
benefit.
|interfaces_info| should thus only contain data about an interface that
contains paths or is needed by *other* interfaces, e.g., path data (to abstract
the compiler from OS-specific file paths) or public data (to avoid having to
read other interfaces unnecessarily).
It should *not* contain full information about an interface (e.g., all
extended attributes), as this would cause unnecessary rebuilds.

|interfaces_info| is a dict, keyed by |interface_name|.

Current keys are:
* dependencies:
    'implements_interfaces': targets of 'implements' statements
    'referenced_interfaces': reference interfaces that are introspected
                             (currently just targets of [PutForwards])

* inheritance:
    'ancestors': all ancestor interfaces
    'inherited_extended_attributes': inherited extended attributes
                                     (all controlling memory management)

* public:
    'is_callback_interface': bool, callback interface or not
    'implemented_as': value of [ImplementedAs=...] on interface (C++ class name)

* paths:
    'full_path': path to the IDL file, so can lookup an IDL by interface name
    'include_path': path for use in C++ #include directives
    'dependencies_full_paths': paths to dependencies (for merging into main)
    'dependencies_include_paths': paths for use in C++ #include directives
    'dependencies_other_component_full_paths':
        paths to dependencies (cannot merge because of other component)
    'dependencies_other_component_include_paths':
        paths for use in C++ #include directives because of dependencies in
        other component

Note that all of these are stable information, unlikely to change without
moving or deleting files (hence requiring a full rebuild anyway) or significant
code changes (for inherited extended attributes).

Design doc: http://www.chromium.org/developers/design-documents/idl-build
"""

# pylint: disable=relative-import

import optparse
import sys

from collections import defaultdict
from utilities import idl_filename_to_component
from utilities import merge_dict_recursively
from utilities import read_pickle_files
from utilities import shorten_union_name
from utilities import write_pickle_file

INHERITED_EXTENDED_ATTRIBUTES = set([
    'ActiveScriptWrappable',
    'DependentLifetime',
    'LegacyUnenumerableNamedProperties',
])

# Main variable (filled in and exported)
interfaces_info = {}

# Auxiliary variables (not visible to future build steps)
partial_interface_files = defaultdict(lambda: {
    'full_paths': [],
    'include_paths': [],
})
parent_interfaces = {}
inherited_extended_attributes_by_interface = {}  # interface name -> extended attributes


class IdlInterfaceFileNotFoundError(Exception):
    """Raised if the IDL file implementing an interface cannot be found."""
    pass


def parse_options():
    usage = 'Usage: %prog [InfoIndividual.pickle]... [Info.pickle]'
    parser = optparse.OptionParser(usage=usage)

    return parser.parse_args()


def dict_of_dicts_of_lists_update_or_append(existing, other):
    """Updates an existing dict of dicts of lists, or appends to lists if key already present.

    Needed for merging partial_interface_files across components.
    """
    for key, value in other.iteritems():
        if key not in existing:
            existing[key] = value
            continue
        existing_value = existing[key]
        for inner_key, inner_value in value.iteritems():
            existing_value[inner_key].extend(inner_value)


################################################################################
# Computations
################################################################################

def compute_inheritance_info(interface_name):
    """Compute inheritance information, namely ancestors and inherited extended attributes."""
    def generate_ancestors(interface_name):
        while interface_name in parent_interfaces:
            interface_name = parent_interfaces[interface_name]
            yield interface_name

    ancestors = list(generate_ancestors(interface_name))
    inherited_extended_attributes = inherited_extended_attributes_by_interface[interface_name]
    for ancestor in ancestors:
        # Ancestors may not be present, notably if an ancestor is a generated
        # IDL file and we are running this script from run-bindings-tests,
        # where we don't generate these files.
        ancestor_extended_attributes = inherited_extended_attributes_by_interface.get(ancestor, {})
        inherited_extended_attributes.update(ancestor_extended_attributes)

    interfaces_info[interface_name].update({
        'ancestors': ancestors,
        'inherited_extended_attributes': inherited_extended_attributes,
    })


def compute_global_type_info():
    ancestors = {}
    dictionaries = {}
    component_dirs = {}
    implemented_as_interfaces = {}
    garbage_collected_interfaces = set()
    callback_interfaces = set()

    for interface_name, interface_info in interfaces_info.iteritems():
        component_dirs[interface_name] = idl_filename_to_component(interface_info['full_path'])

        if interface_info['ancestors']:
            ancestors[interface_name] = interface_info['ancestors']
        if interface_info['is_callback_interface']:
            callback_interfaces.add(interface_name)
        if interface_info['is_dictionary']:
            dictionaries[interface_name] = interface_info['is_dictionary']
        if interface_info['implemented_as']:
            implemented_as_interfaces[interface_name] = interface_info['implemented_as']

        inherited_extended_attributes = interface_info['inherited_extended_attributes']
        garbage_collected_interfaces.add(interface_name)

    interfaces_info['ancestors'] = ancestors
    interfaces_info['callback_interfaces'] = callback_interfaces
    interfaces_info['dictionaries'] = dictionaries
    interfaces_info['implemented_as_interfaces'] = implemented_as_interfaces
    interfaces_info['garbage_collected_interfaces'] = garbage_collected_interfaces
    interfaces_info['component_dirs'] = component_dirs


def compute_interfaces_info_overall(info_individuals):
    """Compute information about IDL files.

    Information is stored in global interfaces_info.
    """
    for info in info_individuals:
        merge_dict_recursively(interfaces_info, info['interfaces_info'])
        # Interfaces in one component may have partial interfaces in
        # another component. This is ok (not a layering violation), since
        # partial interfaces are used to *extend* interfaces.
        # We thus need to update or append if already present
        dict_of_dicts_of_lists_update_or_append(
                partial_interface_files, info['partial_interface_files'])

    # Record inheritance information individually
    for interface_name, interface_info in interfaces_info.iteritems():
        extended_attributes = interface_info['extended_attributes']
        inherited_extended_attributes_by_interface[interface_name] = dict(
                (key, value)
                for key, value in extended_attributes.iteritems()
                if key in INHERITED_EXTENDED_ATTRIBUTES)
        parent = interface_info['parent']
        if parent:
            parent_interfaces[interface_name] = parent

    # Once all individual files handled, can compute inheritance information
    # and dependencies

    # Compute inheritance info
    for interface_name in interfaces_info:
        compute_inheritance_info(interface_name)

    # Compute dependencies
    # Move implements info from implement*ed* interface (rhs of 'implements')
    # to implement*ing* interface (lhs of 'implements').
    # Note that moving an 'implements' statement between implementing and
    # implemented files does not change the info (or hence cause a rebuild)!
    for right_interface_name, interface_info in interfaces_info.iteritems():
        for left_interface_name in interface_info['implemented_by_interfaces']:
            interfaces_info[left_interface_name]['implements_interfaces'].append(right_interface_name)
        del interface_info['implemented_by_interfaces']

    # An IDL file's dependencies are partial interface files that extend it,
    # and files for other interfaces that this interfaces implements.
    for interface_name, interface_info in interfaces_info.iteritems():
        partial_interface_paths = partial_interface_files[interface_name]
        partial_interfaces_full_paths = partial_interface_paths['full_paths']
        # Partial interface definitions each need an include, as they are
        # implemented in separate classes from the main interface.
        partial_interfaces_include_paths = partial_interface_paths['include_paths']

        implemented_interfaces = interface_info['implements_interfaces']
        try:
            implemented_interfaces_info = [
                interfaces_info[interface]
                for interface in implemented_interfaces]
        except KeyError as key_name:
            raise IdlInterfaceFileNotFoundError('Could not find the IDL file where the following implemented interface is defined: %s' % key_name)
        implemented_interfaces_full_paths = [
            implemented_interface_info['full_path']
            for implemented_interface_info in implemented_interfaces_info]
        # Implemented interfaces don't need includes, as this is handled in
        # the Blink implementation (they are implemented on |impl| itself,
        # hence header is included in implementing class).
        # However, they are needed for legacy implemented interfaces that
        # are being treated as partial interfaces, until we remove these.
        # http://crbug.com/360435
        implemented_interfaces_include_paths = [
            implemented_interface_info['include_path']
            for implemented_interface_info in implemented_interfaces_info
            if implemented_interface_info['is_legacy_treat_as_partial_interface']]

        dependencies_full_paths = implemented_interfaces_full_paths
        dependencies_include_paths = implemented_interfaces_include_paths
        dependencies_other_component_full_paths = []
        dependencies_other_component_include_paths = []

        component = idl_filename_to_component(interface_info['full_path'])
        for full_path in partial_interfaces_full_paths:
            partial_interface_component = idl_filename_to_component(full_path)
            if component == partial_interface_component:
                dependencies_full_paths.append(full_path)
            else:
                dependencies_other_component_full_paths.append(full_path)

        for include_path in partial_interfaces_include_paths:
            partial_interface_component = idl_filename_to_component(include_path)
            if component == partial_interface_component:
                dependencies_include_paths.append(include_path)
            else:
                dependencies_other_component_include_paths.append(include_path)

        interface_info.update({
            'dependencies_full_paths': dependencies_full_paths,
            'dependencies_include_paths': dependencies_include_paths,
            'dependencies_other_component_full_paths':
                dependencies_other_component_full_paths,
            'dependencies_other_component_include_paths':
                dependencies_other_component_include_paths,
        })

    # Clean up temporary private information
    for interface_info in interfaces_info.itervalues():
        del interface_info['extended_attributes']
        del interface_info['union_types']
        del interface_info['is_legacy_treat_as_partial_interface']

    # Compute global_type_info to interfaces_info so that idl_compiler does
    # not need to always calculate the info in __init__.
    compute_global_type_info()


################################################################################

def main():
    _, args = parse_options()
    # args = Input1, Input2, ..., Output
    interfaces_info_filename = args.pop()
    info_individuals = read_pickle_files(args)

    compute_interfaces_info_overall(info_individuals)
    write_pickle_file(interfaces_info_filename, interfaces_info)


if __name__ == '__main__':
    sys.exit(main())
