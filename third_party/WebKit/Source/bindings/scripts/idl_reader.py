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

"""Read an IDL file or complete IDL interface, producing an IdlDefinitions object.

FIXME: Currently a stub, as part of landing the parser incrementally.
Just create dummy IdlDefinitions objects (no parsing or object building done),
and resolve dependencies.
The interface reader should always return None, indicating that bindings should
not be generated, since the code generator has also not landed yet.
"""

import os.path

import interface_dependency_resolver


class IdlDefinitions():
    """Top-level IDL class.

    In Web IDL spec terms, represents a set of definitions, obtained by parsing
    a set of IDL fragments (i.e., the contents of .idl files).
    See: http://www.w3.org/TR/WebIDL/#idl
    """
    # FIXME: Dummy class; full class hierarchy will be added with parser
    pass


def read_idl_definitions(idl_filename, interface_dependencies_filename, additional_idl_filenames):
    """Returns an IdlDefinitions object for an IDL file, including all dependencies."""
    basename = os.path.basename(idl_filename)

    idl_definitions = read_idl_file(idl_filename)
    should_generate_bindings = interface_dependency_resolver.merge_interface_dependencies(idl_definitions, idl_filename, interface_dependencies_filename, additional_idl_filenames)
    if not should_generate_bindings:
        return None
    # FIXME: turn on validator
    # idl_validator.validate_extended_attributes(idl_definitions, basename, options.idl_attributes_file)
    return idl_definitions


def read_idl_file(idl_filename, verbose=False):
    """Returns an IdlDefinitions object for an IDL file, without any dependencies."""
    # FIXME: Currently returns dummy object, as parser not present yet.
    # parser = BlinkIDLParser(verbose=verbose)
    # file_node = blink_idl_parser.parse_file(parser, idl_filename)
    # return idl_object_builder.file_node_to_idl_definitions(file_node)
    return IdlDefinitions()
