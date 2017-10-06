#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from collections import defaultdict

import hasher
import json5_generator
import name_utilities
import template_expander


def _symbol(tag):
    return name_utilities.cpp_name(tag).replace('-', '_')


class MakeElementTypeHelpersWriter(json5_generator.Writer):
    default_parameters = {
        'Conditional': {},
        'ImplementedAs': {},
        'JSInterfaceName': {},
        'constructorNeedsCreatedByParser': {},
        'interfaceHeaderDir': {},
        'interfaceName': {},
        'noConstructor': {},
        'noTypeHelpers': {},
        'runtimeEnabled': {},
    }
    default_metadata = {
        'attrsNullNamespace': None,
        'export': '',
        'fallbackInterfaceName': '',
        'fallbackJSInterfaceName': '',
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
    }
    filters = {
        'hash': hasher.hash,
        'symbol': _symbol,
    }

    def __init__(self, json5_file_path):
        super(MakeElementTypeHelpersWriter, self).__init__(json5_file_path)

        self.namespace = self.json5_file.metadata['namespace'].strip('"')
        self.fallback_interface = self.json5_file.metadata['fallbackInterfaceName'].strip('"')

        assert self.namespace, 'A namespace is required.'

        basename = self.namespace.lower() + '_element_type_helpers'
        self._outputs = {
            (basename + '.h'): self.generate_helper_header,
            (basename + '.cc'): self.generate_helper_implementation,
        }

        base_element_header = 'core/%s/%s_element.h' % (self.namespace.lower(), self.namespace.lower())
        if not self.snake_case_source_files:
            base_element_header = 'core/%s/%sElement.h' % (self.namespace.lower(), self.namespace)
        self._template_context = {
            'base_element_header': base_element_header,
            'input_files': self._input_files,
            'namespace': self.namespace,
            'tags': self.json5_file.name_dictionaries,
            'elements': set(),
        }

        tags = self._template_context['tags']
        elements = self._template_context['elements']
        interface_counts = defaultdict(int)
        for tag in tags:
            tag['interface'] = self._interface(tag)
            interface_counts[tag['interface']] += 1
            elements.add(tag['interface'])

        for tag in tags:
            tag['multipleTagNames'] = (interface_counts[tag['interface']] > 1 or tag['interface'] == self.fallback_interface)

    @template_expander.use_jinja("templates/element_type_helpers.h.tmpl", filters=filters)
    def generate_helper_header(self):
        return self._template_context

    @template_expander.use_jinja("templates/element_type_helpers.cc.tmpl", filters=filters)
    def generate_helper_implementation(self):
        return self._template_context

    def _interface(self, tag):
        if tag['interfaceName']:
            return tag['interfaceName']
        name = name_utilities.upper_first(tag['name'])
        # FIXME: We shouldn't hard-code HTML here.
        if name == 'HTML':
            name = 'Html'
        dash = name.find('-')
        while dash != -1:
            name = name[:dash] + name[dash + 1].upper() + name[dash + 2:]
            dash = name.find('-')
        return '%s%sElement' % (self.namespace, name)

if __name__ == "__main__":
    json5_generator.Maker(MakeElementTypeHelpersWriter).main()
