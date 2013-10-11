#!/usr/bin/env python
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

import sys

import hasher
import in_generator
import name_utilities
import template_expander

from in_file import InFile


def _symbol(entry):
    return entry['name'].replace('-', '_')


class MakeQualifiedNamesWriter(in_generator.Writer):
    defaults = {
    }
    default_parameters = {
        'namespace': '',
        'namespaceURI': '',
        'attrsNullNamespace': None,
    }
    filters = {
        'hash': hasher.hash,
        'to_macro_style': name_utilities.to_macro_style,
        'symbol': _symbol,
    }

    def __init__(self, in_file_paths, enabled_conditions):
        super(MakeQualifiedNamesWriter, self).__init__(None, enabled_conditions)
        assert len(in_file_paths) == 2, 'MakeQualifiedNamesWriter requires 2 in files, got %d.' % len(in_file_paths)
        tags_in_file = InFile.load_from_files([in_file_paths[0]], self.defaults, self.valid_values, self.default_parameters)
        attrs_in_file = InFile.load_from_files([in_file_paths[1]], self.defaults, self.valid_values, self.default_parameters)

        namespace = tags_in_file.parameters['namespace'].strip('"')
        assert namespace == attrs_in_file.parameters['namespace'].strip('"'), 'Both in files must have the same namespace.'

        namespace_uri = tags_in_file.parameters['namespaceURI'].strip('"')
        assert namespace_uri == attrs_in_file.parameters['namespaceURI'].strip('"'), 'Both in files must have the same namespaceURI.'

        self._outputs = {
            (namespace + "Names.h"): self.generate_header,
            (namespace + "Names.cpp"): self.generate_implementation,
        }
        self._template_context = {
            'namespace': namespace,
            'namespace_uri': namespace_uri,
            'tags': tags_in_file.name_dictionaries,
            'attrs': attrs_in_file.name_dictionaries,
        }

    @template_expander.use_jinja("MakeQualifiedNames.h.tmpl", filters=filters)
    def generate_header(self):
        return self._template_context

    @template_expander.use_jinja("MakeQualifiedNames.cpp.tmpl", filters=filters)
    def generate_implementation(self):
        return self._template_context


if __name__ == "__main__":
    in_generator.Maker(MakeQualifiedNamesWriter).main(sys.argv)
