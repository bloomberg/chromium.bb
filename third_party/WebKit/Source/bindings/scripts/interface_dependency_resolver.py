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

"""Resolve interface dependencies, producing a merged IdlDefinitions object.

This library computes interface dependencies (partial interfaces and
implements), reads the dependency files, and merges them to the IdlDefinitions
for the main IDL file, producing an IdlDefinitions object representing the
entire interface.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler#TOC-Dependency-resolution
"""

import os.path
import cPickle as pickle

# The following extended attributes can be applied to a dependency interface,
# and are then applied to the individual members when merging.
# Note that this moves the extended attribute from the interface to the member,
# which changes the semantics and yields different code than the same extended
# attribute on the main interface.
DEPENDENCY_EXTENDED_ATTRIBUTES = set([
    'Conditional',
    'PerContextEnabled',
    'RuntimeEnabled',
])


class InterfaceNotFoundError(Exception):
    """Raised if (partial) interface not found in target IDL file."""
    pass


class InvalidPartialInterfaceError(Exception):
    """Raised if a file listed as a partial interface is not in fact so."""
    pass


class InterfaceDependencyResolver(object):
    def __init__(self, interfaces_info, reader):
        """Initialize dependency resolver.

        Args:
            interfaces_info:
                dict of interfaces information, from compute_dependencies.py
            reader:
                IdlReader, used for reading dependency files
        """
        self.interfaces_info = interfaces_info
        self.reader = reader

    def resolve_dependencies(self, definitions, interface_name):
        """Resolve dependencies, merging them into IDL definitions of main file.

        Dependencies consist of 'partial interface' for the same interface as
        in the main file, and other interfaces that this interface 'implements'.

        Modifies definitions in place by adding parsed dependencies.

        Args:
            definitions: IdlDefinitions object, modified in place
            interface_name:
                name of interface whose dependencies are being resolved
        """
        # The Blink IDL filenaming convention is that the file
        # <interface_name>.idl MUST contain the interface "interface_name" or
        # exception "interface_name", unless it is a dependency (e.g.,
        # 'partial interface Foo' can be in FooBar.idl).
        try:
            target_interface = definitions.interfaces[interface_name]
        except KeyError:
            raise InterfaceNotFoundError('Could not find interface or exception "{0}" in {0}.idl'.format(interface_name))

        if interface_name not in self.interfaces_info:
            # No dependencies, nothing to do
            return

        interface_info = self.interfaces_info[interface_name]
        if 'inherited_extended_attributes' in interface_info:
            target_interface.extended_attributes.update(
                interface_info['inherited_extended_attributes'])

        merge_interface_dependencies(target_interface,
                                     interface_info['dependencies_full_paths'],
                                     self.reader)

        for referenced_interface_name in interface_info['referenced_interfaces']:
            referenced_definitions = self.reader.read_idl_definitions(
                self.interfaces_info[referenced_interface_name]['full_path'])
            definitions.interfaces.update(referenced_definitions.interfaces)


def merge_interface_dependencies(target_interface, dependency_idl_filenames, reader):
    """Merge dependencies ('partial interface' and 'implements') in dependency_idl_filenames into target_interface.

    No return: modifies target_interface in place.
    """
    # Sort so order consistent, so can compare output from run to run.
    for dependency_idl_filename in sorted(dependency_idl_filenames):
        dependency_interface_basename, _ = os.path.splitext(os.path.basename(dependency_idl_filename))
        definitions = reader.read_idl_file(dependency_idl_filename)

        for dependency_interface in definitions.interfaces.itervalues():
            # Dependency files contain either partial interfaces for
            # the (single) target interface, in which case the interface names
            # must agree, or interfaces that are implemented by the target
            # interface, in which case the interface names differ.
            if (dependency_interface.is_partial and
                dependency_interface.name != target_interface.name):
                raise InvalidPartialInterfaceError('%s is not a partial interface of %s. There maybe a bug in the the dependency generator (compute_dependencies.py).' % (dependency_idl_filename, target_interface.name))
            merge_dependency_interface(target_interface, dependency_interface, dependency_interface_basename)


def merge_dependency_interface(target_interface, dependency_interface, dependency_interface_basename):
    """Merge dependency_interface into target_interface.

    Merging consists of storing certain interface-level data in extended
    attributes of the *members* (because there is no separate dependency
    interface post-merging), then concatenating the lists.

    The data storing consists of:
    * applying certain extended attributes from the dependency interface
      to its members
    * storing the C++ class of the implementation in an internal
      extended attribute of each member, [ImplementedBy]

    No return: modifies target_interface in place.
    """
    merged_extended_attributes = dict(
        (key, value)
        for key, value in dependency_interface.extended_attributes.iteritems()
        if key in DEPENDENCY_EXTENDED_ATTRIBUTES)

    # C++ class name of the implementation, stored in [ImplementedBy], which
    # defaults to the basename of dependency IDL file.
    # This can be overridden by [ImplementedAs] on the dependency interface.
    # Note that [ImplementedAs] is used with different meanings on interfaces
    # and members:
    # for Blink class name and function name (or constant name), respectively.
    # Thus we do not want to copy this from the interface to the member, but
    # instead extract it and handle it separately.
    merged_extended_attributes['ImplementedBy'] = (
        dependency_interface.extended_attributes.get(
            'ImplementedAs', dependency_interface_basename))

    def merge_lists(source_list, target_list):
        for member in source_list:
            member.extended_attributes.update(merged_extended_attributes)
        target_list.extend(source_list)

    merge_lists(dependency_interface.attributes, target_interface.attributes)
    merge_lists(dependency_interface.constants, target_interface.constants)
    merge_lists(dependency_interface.operations, target_interface.operations)
