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

FIXME: Currently a stub, as part of landing the parser incrementally.
Just computes dependencies, and should always return None, indicating
bindings should not be generated.
Does not read IDL files or merge IdlDefinitions yet.
"""

import os.path


def merge_interface_dependencies(idl_definitions, idl_filename, interface_dependencies_filename, additional_idl_filenames):
    """Merges dependencies into an existing IDL document.

    Modifies idl_definitions in place by adding parsed dependencies, and checks
    whether bindings should be generated, returning bool.

    FIXME: stub: parser not implemented yet

    Arguments:
        idl_definitions: IdlDefinitions object, modified in place
        idl_filename: filename of main IDL file for the interface
        interface_dependencies_file: filename of dependencies file (produced by compute_dependencies.py)
        additional_idl_files: list of additional files, not listed in interface_dependencies_file, for which bindings should nonetheless be generated
    Returns:
        bool, whether bindings should be generated or not.
    """
    basename = os.path.basename(idl_filename)
    interface_name, _ = os.path.splitext(basename)

    dependency_idl_filenames = compute_dependency_idl_files(basename, interface_dependencies_filename, additional_idl_filenames)
    if dependency_idl_filenames is None:
        return False
    # FIXME: currently dependency_idl_files *must* be None (indicating that
    # dummy .cpp and .h files should be generated), as actual parser not
    # present yet.
    raise RuntimeError('Stub: parser not implemented yet')


def compute_dependency_idl_files(target_idl_basename, interface_dependencies_filename, additional_idl_filenames):
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
    if interface_dependencies_filename is None:
        return []

    # The format of the interface dependencies file is:
    #
    # Document.idl P.idl
    # Event.idl
    # Window.idl Q.idl R.idl S.idl
    # ...
    #
    # The above indicates that:
    # Document.idl depends on P.idl,
    # Event.idl depends on no other IDL files, and
    # Window.idl depends on Q.idl, R.idl, and S.idl.
    #
    # The head entries (first IDL file on a line) are the files that should
    # have bindings generated.
    #
    # An IDL file that is a dependency of another IDL file (e.g., P.idl in the
    # above example) does not have its own line in the dependency file:
    # dependencies do not have bindings generated, and do not have their
    # own dependencies.
    with open(interface_dependencies_filename) as interface_dependencies_file:
        for line in interface_dependencies_file:
            idl_filename, _, dependency_filenames = line.partition(' ')
            if os.path.basename(idl_filename) == target_idl_basename:
                return dependency_filenames.split()

    # additional_idl_files is a list of IDL files which should not be included
    # in DerivedSources*.cpp, and hence are not listed in the interface
    # dependencies file, but for which we should generate .cpp and .h files.
    additional_idl_basenames = set(map(os.path.basename, additional_idl_filenames))
    if target_idl_basename in additional_idl_basenames:
        return []

    return None
