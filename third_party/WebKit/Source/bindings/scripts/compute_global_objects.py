#!/usr/bin/python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Compute global objects.

Global objects are defined by interfaces with [Global] or [PrimaryGlobal] on
their definition: http://heycam.github.io/webidl/#Global

Design document: http://www.chromium.org/developers/design-documents/idl-build
"""

import optparse
import os
import sys

from utilities import get_file_contents, idl_filename_to_interface_name, get_interface_extended_attributes_from_idl, read_file_to_list, write_pickle_file

GLOBAL_EXTENDED_ATTRIBUTES = frozenset([
    'Global',
    'PrimaryGlobal',
])


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
    if len(args) != 1:
        parser.error('Must specify a single output pickle filename as argument.')

    return options, args[0]


def idl_file_to_global_names(idl_filename):
    """Returns global names, if any, for an IDL file.

    If the [Global] or [PrimaryGlobal] extended attribute is declared with an
    identifier list argument, then those identifiers are the interface's global
    names; otherwise, the interface has a single global name, which is the
    interface's identifier (http://heycam.github.io/webidl/#Global).
    """
    interface_name = idl_filename_to_interface_name(idl_filename)
    full_path = os.path.realpath(idl_filename)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)

    global_keys = GLOBAL_EXTENDED_ATTRIBUTES.intersection(
        extended_attributes.iterkeys())
    if not global_keys:
        return
    if len(global_keys) > 1:
        raise ValueError('The [Global] and [PrimaryGlobal] extended attributes '
                         'MUST NOT be declared on the same interface.')
    global_key = next(iter(global_keys))

    global_value = extended_attributes[global_key]
    if global_value:
        # FIXME: In spec names are comma-separated, which makes parsing very
        # difficult (https://www.w3.org/Bugs/Public/show_bug.cgi?id=24959).
        return global_value.split('&')
    return [interface_name]


def interface_name_global_names(idl_files):
    """Yields pairs (interface_name, global_names) found in IDL files."""
    for idl_filename in idl_files:
        interface_name = idl_filename_to_interface_name(idl_filename)
        global_names = idl_file_to_global_names(idl_filename)
        if global_names:
            yield interface_name, global_names


################################################################################

def main():
    options, global_objects_filename = parse_options()

    # Input IDL files are passed in a file, due to OS command line length
    # limits. This is generated at GYP time, which is ok b/c files are static.
    idl_files = read_file_to_list(options.idl_files_list)

    write_pickle_file(global_objects_filename,
                      dict(interface_name_global_names(idl_files)),
                      options.write_file_only_if_changed)


if __name__ == '__main__':
    sys.exit(main())
