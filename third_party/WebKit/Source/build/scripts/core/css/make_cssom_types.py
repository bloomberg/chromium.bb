#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

from core.css import css_properties
import json5_generator
from name_utilities import enum_for_css_keyword
import template_expander


class CSSOMTypesWriter(css_properties.CSSProperties):
    """
    Generates CSSOMTypes.cpp and CSSOMKeywords.cpp. These classes provide
    utility methods for determining whether a given CSSStyleValue or
    CSSKeywordValue is valid for a given CSS property. The header files live in
    core/css/cssom.
    """
    def __init__(self, json5_file_path):
        super(CSSOMTypesWriter, self).__init__(json5_file_path)

        for property_ in self._properties.values():
            types = []
            # Expand types
            for single_type in property_['typedom_types']:
                if single_type == 'Image':
                    types.append('URLImage')
                else:
                    types.append(single_type)
            property_['typedom_types'] = types

            # Generate Keyword ID values from keywords.
            property_['keywordIDs'] = map(
                enum_for_css_keyword, property_['keywords'])

        self._outputs = {
            'CSSOMTypes.cpp': self.generate_types,
            'CSSOMKeywords.cpp': self.generate_keywords,
        }

    @template_expander.use_jinja('core/css/templates/CSSOMTypes.cpp.tmpl')
    def generate_types(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }

    @template_expander.use_jinja('core/css/templates/CSSOMKeywords.cpp.tmpl')
    def generate_keywords(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }

if __name__ == '__main__':
    json5_generator.Maker(CSSOMTypesWriter).main()
