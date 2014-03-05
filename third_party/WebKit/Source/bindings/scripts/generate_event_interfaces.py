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

"""Generate event interfaces .in file (EventInterfaces.in).

The event interfaces .in file contains a list of all Event interfaces, i.e.,
all interfaces that inherit from Event, including Event itself,
together with certain extended attributes.

This list is used in core/ to generate EventFactory and EventNames.
The .in format is documented in build/scripts/in_file.py.
"""

import optparse
import cPickle as pickle
import os
import posixpath
import sys

from utilities import get_file_contents, write_file, get_interface_extended_attributes_from_idl

interfaces_info = {}
extended_attributes_by_interface = {}  # interface name -> extended attributes


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--event-names-file', help='output file')
    parser.add_option('--interfaces-info-file', help='output pickle file')
    parser.add_option('--write-file-only-if-changed', type='int', help='if true, do not write an output file if it would be identical to the existing one, which avoids unnecessary rebuilds in ninja')

    options, args = parser.parse_args()
    if options.event_names_file is None:
        parser.error('Must specify an output file using --event-names-file.')
    if options.interfaces_info_file is None:
        parser.error('Must specify an input file using --interfaces-info-file.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether file is only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)
    if args:
        parser.error('No arguments allowed, but %d given.' % len(args))
    return options


def store_event_extended_attributes():
    event_interfaces = set(
        interface_name
        for interface_name, interface_info in interfaces_info.iteritems()
        if (interface_name == 'Event' or
            ('ancestors' in interface_info and
             interface_info['ancestors'][-1] == 'Event')))
    for interface_name in event_interfaces:
        interface_info = interfaces_info[interface_name]
        idl_file_contents = get_file_contents(interface_info['full_path'])
        extended_attributes_by_interface[interface_name] = get_interface_extended_attributes_from_idl(idl_file_contents)


def write_event_names_file(destination_filename, only_if_changed):
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


################################################################################

def main():
    options = parse_options()
    with open(options.interfaces_info_file) as interfaces_info_file:
        interfaces_info.update(pickle.load(interfaces_info_file))
    store_event_extended_attributes()
    write_event_names_file(options.event_names_file,
                           options.write_file_only_if_changed)


if __name__ == '__main__':
    sys.exit(main())
