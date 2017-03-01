#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import css_properties
import json5_generator
from name_utilities import lower_first
import template_expander


class CSSPropertyMetadataWriter(css_properties.CSSProperties):
    filters = {
        'lower_first': lower_first,
    }

    def __init__(self, json5_file_path):
        super(CSSPropertyMetadataWriter, self).__init__(json5_file_path)
        self._outputs = {'CSSPropertyMetadata.cpp': self.generate_css_property_metadata_cpp}

    @template_expander.use_jinja('CSSPropertyMetadata.cpp.tmpl', filters=filters)
    def generate_css_property_metadata_cpp(self):
        return {
            'properties': self._properties,
            'switches': [('descriptor_only', 'isDescriptorOnly'),
                         ('interpolable', 'isInterpolableProperty'),
                         ('inherited', 'isInheritedProperty'),
                         ('supports_percentage', 'propertySupportsPercentage'),
                        ],
            'first_enum_value': self._first_enum_value,
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyMetadataWriter).main()
