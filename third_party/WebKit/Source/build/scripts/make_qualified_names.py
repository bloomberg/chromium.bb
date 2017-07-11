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
import json5_generator
import name_utilities
import template_expander

from json5_generator import Json5File


def _symbol(entry):
    return entry['name'].replace('-', '_')


class MakeQualifiedNamesWriter(json5_generator.Writer):
    default_parameters = {}
    default_metadata = {
        'attrsNullNamespace': None,
        'export': '',
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
    }
    filters = {
        'hash': hasher.hash,
        'symbol': _symbol,
        'to_macro_style': name_utilities.to_macro_style,
    }

    def __init__(self, json5_file_paths):
        super(MakeQualifiedNamesWriter, self).__init__(None)
        assert len(json5_file_paths) <= 2, 'MakeQualifiedNamesWriter requires at most 2 in files, got %d.' % len(json5_file_paths)

        if len(json5_file_paths) == 2:
            self.tags_json5_file = Json5File.load_from_files(
                [json5_file_paths.pop(0)], self.default_metadata, self.default_parameters)
        else:
            self.tags_json5_file = None

        self.attrs_json5_file = Json5File.load_from_files([json5_file_paths.pop()], self.default_metadata, self.default_parameters)

        self.namespace = self._metadata('namespace')

        namespace_prefix = self._metadata('namespacePrefix') or self.namespace.lower()
        namespace_uri = self._metadata('namespaceURI')

        use_namespace_for_attrs = self.attrs_json5_file.metadata['attrsNullNamespace'] is None

        self._outputs = {
            (self.namespace + "Names.h"): self.generate_header,
            (self.namespace + "Names.cpp"): self.generate_implementation,
        }
        self._template_context = {
            'attrs': self.attrs_json5_file.name_dictionaries,
            'export': self._metadata('export'),
            'namespace': self.namespace,
            'namespace_prefix': namespace_prefix,
            'namespace_uri': namespace_uri,
            'tags': self.tags_json5_file.name_dictionaries if self.tags_json5_file else [],
            'use_namespace_for_attrs': use_namespace_for_attrs,
        }

    def _metadata(self, name):
        metadata = self.attrs_json5_file.metadata[name].strip('"')
        if self.tags_json5_file:
            assert metadata == self.tags_json5_file.metadata[name].strip('"'), 'Both files must have the same %s.' % name
        return metadata

    @template_expander.use_jinja('templates/MakeQualifiedNames.h.tmpl', filters=filters)
    def generate_header(self):
        return self._template_context

    @template_expander.use_jinja('templates/MakeQualifiedNames.cpp.tmpl', filters=filters)
    def generate_implementation(self):
        return self._template_context


if __name__ == "__main__":
    json5_generator.Maker(MakeQualifiedNamesWriter).main()
