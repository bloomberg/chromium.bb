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

"""Validate extended attributes."""


import re


class IDLExtendedAttributeFileFormatError(Exception):
    pass


class IDLInvalidExtendedAttributeError(Exception):
    pass


class IDLExtendedAttributeValidator:
    def __init__(self, extended_attributes_filename):
        self.valid_extended_attributes = read_extended_attributes_file(extended_attributes_filename)

    def validate_extended_attributes(self, definitions):
        # FIXME: this should be done when parsing the file, rather than after.
        for interface in definitions.interfaces.itervalues():
            self.validate_extended_attributes_node(interface)
            for attribute in interface.attributes:
                self.validate_extended_attributes_node(attribute)
            for operation in interface.operations:
                self.validate_extended_attributes_node(operation)
                for argument in operation.arguments:
                    self.validate_extended_attributes_node(argument)

    def validate_extended_attributes_node(self, node):
        for name, values_string in node.extended_attributes.iteritems():
            self.validate_name_values_string(name, values_string)

    def validate_name_values_string(self, name, values_string):
        if name == 'ImplementedBy':  # attribute added when merging interfaces
            return
        if name not in self.valid_extended_attributes:
            raise IDLInvalidExtendedAttributeError('Unknown extended attribute [%s]' % name)
        valid_values = self.valid_extended_attributes[name]
        if '*' in valid_values:  # wildcard, any value ok
            return
        if values_string is None:
            value_list = [None]
        else:
            value_list = re.split('[|&]', values_string)
        for value in value_list:
            if value not in valid_values:
                raise IDLInvalidExtendedAttributeError('Invalid value "%s" found in extended attribute [%s=%s]' % (value, name, values_string))


def read_extended_attributes_file(extended_attributes_filename):
    valid_extended_attributes = {}
    with open(extended_attributes_filename) as extended_attributes_file:
        for line in extended_attributes_file:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            name, _, values_string = map(str.strip, line.partition('='))
            if not name:
                raise IDLExtendedAttributeFileFormatError('The format of %s is wrong, in line "%s"' % (extended_attributes_filename, line))
            valid_extended_attributes[name] = set()
            value_list = values_string.split('|')
            if not value_list:
                valid_extended_attributes[name].add(None)
                continue
            for value in value_list:
                value = value.strip()
                if not value:
                    value = None
                valid_extended_attributes[name].add(value)
    return valid_extended_attributes
