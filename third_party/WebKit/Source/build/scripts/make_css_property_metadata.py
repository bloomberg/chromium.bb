#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import css_properties
import in_generator
import template_expander


class CSSPropertyMetadataWriter(css_properties.CSSProperties):
    def __init__(self, in_file_path):
        super(CSSPropertyMetadataWriter, self).__init__(in_file_path)
        self._outputs = {'CSSPropertyMetadata.cpp': self.generate_css_property_metadata_cpp}

    @template_expander.use_jinja('CSSPropertyMetadata.cpp.tmpl')
    def generate_css_property_metadata_cpp(self):
        return {
            'properties': self._properties,
            'switches': [('animatable', 'isAnimatableProperty'),
                         ('inherited', 'isInheritedProperty'),
                        ],
        }


if __name__ == '__main__':
    in_generator.Maker(CSSPropertyMetadataWriter).main(sys.argv)
