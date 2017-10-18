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

from collections import namedtuple, Counter
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
        for property_ in self.properties().values():
            if property_['api_class'] is None:
                continue
            methods = []
            for method_name in property_['api_methods']:
                methods.append(self._api_methods[method_name])
            property_['api_methods'] = methods
            classname = self.get_classname(property_)
            assert classname is not None
            # Functions should only be declared on the API classes if they are
            # implemented and not shared (denoted by api_class = true. Shared
            # classes are denoted by api_class = "some string").
            property_['should_declare_application_functions'] = \
                property_['api_class'] \
                and isinstance(property_['api_class'], types.BooleanType) \
                and property_['is_property'] \
                and not property_['use_handlers_for'] \
                and not property_['longhands'] \
                and not property_['direction_aware_options'] \
                and not property_['builder_skip']
            self._outputs[classname + '.h'] = (
                self.generate_property_api_h_builder(classname, property_))

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
        classname_counts = Counter(
            property_['api_class'] for property_ in self.properties().values()
            if property_['api_class'] not in (None, True))
        for property_ in self.properties().values():
            api_class = property_['api_class']
            if api_class not in (None, True):
                assert classname_counts[api_class] != 1,\
                    ("Unique api_class '" + api_class + "' defined on '" +
                     property_['name'] + "' property. api_class as string is " +
                     "reserved for grouped properties. Did you mean " +
                     "'api_class: true'?")

if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIHeadersWriter).main()
