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

It also checks whether a file should have bindings generated, or whether
instead it is just a dependency.
"""

import os.path


class InterfaceNotFoundError(Exception):
    """Raised if (partial) interface not found in target IDL file."""
    pass


class InvalidPartialInterfaceError(Exception):
    """Raised if a file listed as a partial interface is not in fact so."""
    pass


class InterfaceDependencyResolver:
    def __init__(self, interface_dependencies_filename, additional_idl_filenames, reader):
        """Inits dependency resolver.

        Args:
            interface_dependencies_filename:
                filename of dependencies file (produced by
                compute_dependencies.py)
            additional_idl_filenames:
                list of additional files, not listed in
                interface_dependencies_file, for which bindings should
                nonetheless be generated
            reader:
                IdlReader, used for reading dependency files
        """
        self.interface_dependencies = read_interface_dependencies_file(interface_dependencies_filename)
        self.additional_interfaces = set()
        for filename in additional_idl_filenames:
            basename = os.path.basename(filename)
            interface_name, _ = os.path.splitext(basename)
            self.additional_interfaces.add(interface_name)
        self.reader = reader

    def resolve_dependencies(self, definitions, interface_name):
        """Resolves dependencies, merging them into IDL definitions of main file.

        Dependencies consist of 'partial interface' for the same interface as
        in the main file, and other interfaces that this interface 'implements'.

        Modifies definitions in place by adding parsed dependencies, and checks
        whether bindings should be generated, returning bool.

        Args:
            definitions: IdlDefinitions object, modified in place
            idl_filename: filename of main IDL file for the interface
        Returns:
            bool, whether bindings should be generated or not.
        """
        dependency_idl_filenames = self.compute_dependency_idl_files(interface_name)
        if dependency_idl_filenames is None:
            return False
        # The Blink IDL filenaming convention is that the file
        # <interface_name>.idl MUST contain the interface "interface_name" or
        # exception "interface_name", unless it is a dependency (e.g.,
        # 'partial interface Foo' can be in FooBar.idl).
        if interface_name in definitions.exceptions:
            # Exceptions do not have dependencies, so no merging necessary
            return definitions
        try:
            target_interface = definitions.interfaces[interface_name]
        except KeyError:
            raise InterfaceNotFoundError('Could not find interface or exception "{0}" in {0}.idl'.format(interface_name))
        merge_interface_dependencies(target_interface, dependency_idl_filenames, self.reader)

        return definitions

    def compute_dependency_idl_files(self, target_interface_name):
        """Returns list of IDL file dependencies for a given main IDL file.

        - Returns a list of IDL files on which a given IDL file depends,
          possibly empty.
          Dependencies consist of partial interface files and files for other
          interfaces that the given interface implements.
        - Returns an empty list also if the given IDL file is an additional IDL
          file.
        - Otherwise, return None. This happens when the given IDL file is a
          dependency, for which we don't want to generate bindings.
        """
        if target_interface_name in self.interface_dependencies:
            return self.interface_dependencies[target_interface_name]

        # additional_interfaces is a list of interfaces that should not be
        # included in DerivedSources*.cpp, and hence are not listed in the
        # interface dependencies file, but for which we should generate .cpp
        # and .h files.
        if target_interface_name in self.additional_interfaces:
            return []

        return None


def read_interface_dependencies_file(interface_dependencies_filename):
    """Reads the interface dependencies file, returns a dict for resolving dependencies.

    The format of the interface dependencies file is:

    Document.idl P.idl
    Event.idl
    Window.idl Q.idl R.idl S.idl
    ...

    The above indicates that:
    Document.idl depends on P.idl,
    Event.idl depends on no other IDL files, and
    Window.idl depends on Q.idl, R.idl, and S.idl.

    The head entries (first IDL file on a line) are the files that should
    have bindings generated.

    An IDL file that is a dependency of another IDL file (e.g., P.idl in the
    above example) does not have its own line in the dependency file:
    dependencies do not have bindings generated, and do not have their
    own dependencies.

    Args:
        interface_dependencies_filename: filename of file in above format
    Returns:
        dict of interface_name -> dependency_filenames
    """
    interface_dependencies = {}
    with open(interface_dependencies_filename) as interface_dependencies_file:
        for line in interface_dependencies_file:
            idl_filename, _, dependency_filenames_string = line.partition(' ')
            idl_basename = os.path.basename(idl_filename)
            interface_name, _ = os.path.splitext(idl_basename)
            dependency_filenames = dependency_filenames_string.split()
            interface_dependencies[interface_name] = dependency_filenames
    return interface_dependencies


def merge_interface_dependencies(target_interface, dependency_idl_filenames, reader):
    """Merge dependencies ('partial interface' and 'implements') in dependency_idl_filenames into target_interface.

    No return: modifies target_document in place.
    """
    # Sort so order consistent, so can compare output from run to run.
    for dependency_idl_filename in sorted(dependency_idl_filenames):
        dependency_interface_name, _ = os.path.splitext(os.path.basename(dependency_idl_filename))
        definitions = reader.read_idl_file(dependency_idl_filename)

        for dependency_interface in definitions.interfaces.itervalues():
            # Dependency files contain either partial interfaces for
            # the (single) target interface, in which case the interface names
            # must agree, or interfaces that are implemented by the target
            # interface, in which case the interface names differ.
            if dependency_interface.is_partial and dependency_interface.name != target_interface.name:
                raise InvalidPartialInterfaceError('%s is not a partial interface of %s. There maybe a bug in the the dependency generator (compute_depedencies.py).' % (dependency_idl_filename, target_interface.name))
            if 'ImplementedAs' in dependency_interface.extended_attributes:
                del dependency_interface.extended_attributes['ImplementedAs']
            merge_dependency_interface(target_interface, dependency_interface, dependency_interface_name)


def merge_dependency_interface(target_interface, dependency_interface, dependency_interface_name):
    """Merge dependency_interface into target_interface.

    No return: modifies target_interface in place.
    """
    def merge_lists(source_list, target_list):
        for element in source_list:
            # FIXME: remove check for LegacyImplementedInBaseClass when this
            # attribute is removed
            if 'LegacyImplementedInBaseClass' not in dependency_interface.extended_attributes:
                element.extended_attributes['ImplementedBy'] = dependency_interface_name
            element.extended_attributes.update(dependency_interface.extended_attributes)
            target_list.append(element)

    merge_lists(dependency_interface.attributes, target_interface.attributes)
    merge_lists(dependency_interface.constants, target_interface.constants)
    merge_lists(dependency_interface.operations, target_interface.operations)
