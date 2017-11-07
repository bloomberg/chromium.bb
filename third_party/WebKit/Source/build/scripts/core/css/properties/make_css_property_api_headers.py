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
from make_css_property_api_base import CSSPropertyAPIWriter


class ApiMethod(namedtuple('ApiMethod', 'name,return_type,parameters')):
    pass


class CSSPropertyAPIHeadersWriter(CSSPropertyAPIWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertyAPIHeadersWriter, self).__init__(json5_file_paths)
        assert len(json5_file_paths) == 3,\
            ('CSSPropertyAPIHeadersWriter requires 3 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        # Map of API method name -> (return_type, parameters)
        self._api_methods = {}
        api_methods = json5_generator.Json5File.load_from_files(
            [json5_file_paths[2]])
        for api_method in api_methods.name_dictionaries:
            self._api_methods[api_method['name']] = ApiMethod(
                name=api_method['name'],
                return_type=api_method['return_type'],
                parameters=api_method['parameters'],
            )
        self.validate_input()

        self._outputs = {}
        output_dir = sys.argv[sys.argv.index('--output_dir') + 1]
        properties = self.css_properties.longhands
        class_prefix = 'CSSPropertyAPI'
        if 'shorthands' in output_dir:
            properties = self.css_properties.shorthands
            class_prefix = 'CSSShorthandPropertyAPI'
        for property_ in properties:
            if property_['api_class'] is None:
                continue
            property_['unique'] = isinstance(
                property_['api_class'], types.BooleanType)
            property_['api_methods'] = [
                self._api_methods[method_name]
                for method_name in property_['api_methods']
            ]
            class_data = self.get_class(property_, class_prefix)
            # Functions should only be declared on the API classes if they are
            # implemented and not shared (denoted by api_class = true. Shared
            # classes are denoted by api_class = "some string").
            property_['should_declare_application_functions'] = \
                property_['unique'] \
                and property_['is_property'] \
                and not property_['longhands'] \
                and not property_['direction_aware_options'] \
                and not property_['builder_skip']
            self._outputs[class_data.classname + '.h'] = (
                self.generate_property_api_h_builder(
                    class_data.classname, property_))

    def generate_property_api_h_builder(self, api_classname, property_):
        @template_expander.use_jinja(
            'core/css/properties/templates/CSSPropertyAPISubclass.h.tmpl')
        def generate_property_api_h():
            return {
                'input_files': self._input_files,
                'api_classname': api_classname,
                'property': property_,
            }
        return generate_property_api_h

    def validate_input(self):
        # First collect which classes correspond to which properties.
        class_names_to_properties = defaultdict(list)
        for property_ in self.css_properties.properties_including_aliases:
            if property_['api_class'] is None:
                continue
            class_data = self.get_class(property_)
            class_names_to_properties[class_data.classname].append(property_)
        # Check that properties that share class names specify the same method
        # names.
        for properties in class_names_to_properties.values():
            if len(properties) == 1:
                assert properties[0]['api_class'] is True, 'Unique api_class ' \
                    '{} defined on {}. api_class as a string is reserved for ' \
                    'use with properties that share logic. Did you mean ' \
                    'api_class: true?'.format(
                        properties[0]['api_class'], properties[0]['name'])

            api_methods = set(properties[0]['api_methods'])
            for property_ in properties[1:]:
                assert api_methods == set(property_['api_methods']), \
                    'Properties {} and {} share the same api_class but do ' \
                    'not declare the same api_methods. Properties sharing ' \
                    'the same api_class must declare the same list of ' \
                    'api_methods to ensure deterministic code ' \
                    'generation.'.format(
                        properties[0]['name'], property_['name'])


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIHeadersWriter).main()
