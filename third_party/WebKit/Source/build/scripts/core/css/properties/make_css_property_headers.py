#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))
import types

import json5_generator
import template_expander

from collections import defaultdict, namedtuple
from make_css_property_base import CSSPropertyWriter


class PropertyMethod(namedtuple('PropertyMethod', 'name,return_type,parameters')):
    pass


class CSSPropertyHeadersWriter(CSSPropertyWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertyHeadersWriter, self).__init__(json5_file_paths)
        assert len(json5_file_paths) == 3,\
            ('CSSPropertyHeadersWriter requires 3 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        # Map of property method name -> (return_type, parameters)
        self._property_methods = {}
        property_methods = json5_generator.Json5File.load_from_files(
            [json5_file_paths[2]])
        for property_method in property_methods.name_dictionaries:
            self._property_methods[property_method['name']] = PropertyMethod(
                name=property_method['name'],
                return_type=property_method['return_type'],
                parameters=property_method['parameters'],
            )
        self.validate_input()

        self._outputs = {}
        output_dir = sys.argv[sys.argv.index('--output_dir') + 1]
        properties = self.css_properties.longhands
        superclass = 'Longhand'
        if 'shorthands' in output_dir:
            properties = self.css_properties.shorthands
            superclass = 'Shorthand'
        for property_ in properties:
            if property_['property_class'] is None:
                continue
            property_['unique'] = isinstance(
                property_['property_class'], types.BooleanType)
            property_['property_methods'] = [
                self._property_methods[method_name]
                for method_name in property_['property_methods']
            ]
            property_['superclass'] = superclass
            class_data = self.get_class(property_)
            self.calculate_apply_functions_to_declare(property_)
            self._outputs[class_data.classname + '.h'] = (
                self.generate_property_h_builder(
                    class_data.classname, property_))

    def generate_property_h_builder(self, property_classname, property_):
        @template_expander.use_jinja(
            'core/css/properties/templates/CSSPropertySubclass.h.tmpl')
        def generate_property_h():
            return {
                'input_files': self._input_files,
                'property_classname': property_classname,
                'property': property_,
            }
        return generate_property_h

    def calculate_apply_functions_to_declare(self, property_):
        # Functions should only be declared on the property classes if they are
        # implemented and not shared (denoted by property_class = true. Shared
        # classes are denoted by property_class = "some string").
        property_['should_declare_apply_functions'] = \
            property_['unique'] \
            and property_['is_property'] \
            and not property_['longhands'] \
            and not property_['direction_aware_options'] \
            and not property_['builder_skip']
        if property_['custom_apply_functions_all']:
            property_name = property_['upper_camel_name']
            if (property_name in
                    ['Clip', 'ColumnCount', 'ColumnWidth', 'ZIndex']):
                property_['custom_apply'] = "auto"
                property_['custom_apply_args'] = {'auto_identity': 'CSSValueAuto'}
            if property_name == 'ColumnGap':
                property_['custom_apply'] = "auto"
                property_['custom_apply_args'] = {
                    'auto_getter': 'HasNormalColumnGap',
                    'auto_setter': 'SetHasNormalColumnGap',
                    'auto_identity': 'CSSValueNormal'}
        property_['should_implement_apply_functions'] = (
            property_['should_declare_apply_functions'] and
            (not (property_['custom_apply_functions_initial'] and
                  property_['custom_apply_functions_inherit'] and
                  property_['custom_apply_functions_value']) or
             'custom_apply' in property_.keys()))

    def validate_input(self):
        # First collect which classes correspond to which properties.
        class_names_to_properties = defaultdict(list)
        for property_ in self.css_properties.properties_including_aliases:
            if property_['property_class'] is None:
                continue
            class_data = self.get_class(property_)
            class_names_to_properties[class_data.classname].append(property_)
        # Check that properties that share class names specify the same method
        # names.
        for properties in class_names_to_properties.values():
            if len(properties) == 1:
                assert properties[0]['property_class'] is True, 'Unique property_class ' \
                    '{} defined on {}. property_class as a string is reserved for ' \
                    'use with properties that share logic. Did you mean ' \
                    'property_class: true?'.format(
                        properties[0]['property_class'], properties[0]['name'])

            property_methods = set(properties[0]['property_methods'])
            for property_ in properties[1:]:
                assert property_methods == set(property_['property_methods']), \
                    'Properties {} and {} share the same property_class but do ' \
                    'not declare the same property_methods. Properties sharing ' \
                    'the same property_class must declare the same list of ' \
                    'property_methods to ensure deterministic code ' \
                    'generation.'.format(
                        properties[0]['name'], property_['name'])


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyHeadersWriter).main()
