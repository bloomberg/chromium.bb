#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander
import make_style_builder

from name_utilities import enum_for_css_keyword, enum_value_name


class CSSValueIDMappingsWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, json5_file_path):
        super(CSSValueIDMappingsWriter, self).__init__(json5_file_path)
        self._outputs = {
            'CSSValueIDMappingsGenerated.h': self.generate_css_value_mappings,
        }

    @template_expander.use_jinja('templates/CSSValueIDMappingsGenerated.h.tmpl')
    def generate_css_value_mappings(self):
        mappings = {}
        include_paths = set()
        for property_ in self._properties.values():
            if property_['field_template'] in ('keyword', 'multi_keyword'):
                include_paths.update(property_['include_paths'])

                mappings[property_['type_name']] = {
                    'default_value': enum_value_name(property_['default_value']),
                    'mapping': [(enum_value_name(k), enum_for_css_keyword(k)) for k in property_['keywords']],
                }

        return {
            'include_paths': list(sorted(include_paths)),
            'input_files': self._input_files,
            'mappings': mappings,
        }

if __name__ == '__main__':
    json5_generator.Maker(CSSValueIDMappingsWriter).main()
