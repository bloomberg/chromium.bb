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

"""Read an IDL file or complete IDL interface, producing an IdlDefinitions object."""

import os.path

import blink_idl_parser
import idl_definitions_builder
import idl_validator
import interface_dependency_resolver


def read_idl_definitions(idl_filename, interface_dependencies_filename, additional_idl_filenames, idl_attributes_filename, verbose=False):
    """Returns an IdlDefinitions object for an IDL file, including all dependencies."""
    basename = os.path.basename(idl_filename)

    definitions = read_idl_file(idl_filename)
    if interface_dependencies_filename:
        should_generate_bindings = interface_dependency_resolver.resolve_dependencies(definitions, idl_filename, interface_dependencies_filename, additional_idl_filenames, verbose=verbose)
        if not should_generate_bindings:
            return None
    if idl_attributes_filename:
        idl_validator.validate_extended_attributes(definitions, basename, idl_attributes_filename)
    return definitions


def read_idl_file(idl_filename, verbose=False):
    """Returns an IdlDefinitions object for an IDL file, without any dependencies."""
    parser = blink_idl_parser.BlinkIDLParser(verbose=verbose)
    ast = blink_idl_parser.parse_file(parser, idl_filename)
    return idl_definitions_builder.build_idl_definitions_from_ast(ast)
