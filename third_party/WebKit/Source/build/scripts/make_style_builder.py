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
import types

from core.css import css_properties
import json5_generator
from name_utilities import lower_first
import template_expander


def calculate_apply_functions_to_declare(property_):
    property_['should_declare_functions'] = \
        not property_['longhands'] \
        and not property_['direction_aware_options'] \
        and not property_['builder_skip'] \
        and property_['is_property']
    property_['use_property_class_in_stylebuilder'] = \
        property_['should_declare_functions'] \
        and not (property_['custom_apply_functions_initial'] or
                 property_['custom_apply_functions_inherit'] or
                 property_['custom_apply_functions_value'])
    # TODO(crbug.com/751354): Remove this hard coded list of supported
    # properties once all of them have been implemented
    if property_['custom_apply_functions_all']:
        if (property_['upper_camel_name'] in [
                'BackgroundAttachment', 'BackgroundBlendMode', 'BackgroundClip', 'BackgroundImage', 'BackgroundOrigin',
                'BackgroundPositionX', 'BackgroundPositionY', 'BackgroundRepeatX', 'BackgroundRepeatY', 'BackgroundSize',
                'BorderImageOutset', 'BorderImageRepeat', 'BorderImageSlice', 'BorderImageWidth', 'Clip', 'ColumnCount',
                'ColumnGap', 'ColumnWidth', 'MaskSourceType', 'WebkitMaskBoxImageOutset', 'WebkitMaskBoxImageRepeat',
                'WebkitMaskBoxImageSlice', 'WebkitMaskBoxImageWidth', 'WebkitMaskClip', 'WebkitMaskComposite', 'WebkitMaskImage',
                'WebkitMaskOrigin', 'WebkitMaskPositionX', 'WebkitMaskPositionY', 'WebkitMaskRepeatX', 'WebkitMaskRepeatY',
                'WebkitMaskSize', 'ZIndex']):
            property_['use_property_class_in_stylebuilder'] = True


class StyleBuilderWriter(json5_generator.Writer):
    filters = {
        'lower_first': lower_first,
    }

    def __init__(self, json5_file_paths):
        super(StyleBuilderWriter, self).__init__([])
        self._outputs = {
            'StyleBuilderFunctions.h': self.generate_style_builder_functions_h,
            'StyleBuilderFunctions.cpp':
                self.generate_style_builder_functions_cpp,
            'StyleBuilder.cpp': self.generate_style_builder,
        }

        self._json5_properties = css_properties.CSSProperties(json5_file_paths)
        self._input_files = json5_file_paths
        self._properties = self._json5_properties.longhands + \
            self._json5_properties.shorthands
        for property_ in self._properties:
            calculate_apply_functions_to_declare(property_)

    @property
    def css_properties(self):
        return self._json5_properties

    @template_expander.use_jinja('templates/StyleBuilderFunctions.h.tmpl',
                                 filters=filters)
    def generate_style_builder_functions_h(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
        }

    @template_expander.use_jinja('templates/StyleBuilderFunctions.cpp.tmpl',
                                 filters=filters)
    def generate_style_builder_functions_cpp(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'properties_by_id': self._json5_properties.properties_by_id,
        }

    @template_expander.use_jinja(
        'templates/StyleBuilder.cpp.tmpl', filters=filters)
    def generate_style_builder(self):
        return {
            'input_files': self._input_files,
            'properties': self._properties,
            'properties_by_id': self._json5_properties.properties_by_id,
        }


if __name__ == '__main__':
    json5_generator.Maker(StyleBuilderWriter).main()
