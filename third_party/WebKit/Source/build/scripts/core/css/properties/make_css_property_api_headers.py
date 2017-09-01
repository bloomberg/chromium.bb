#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

import json5_generator
import template_expander

from collections import namedtuple, defaultdict
from make_css_property_api_base import CSSPropertyAPIWriter


class ApiMethod(namedtuple('ApiMethod', 'name,return_type,parameters')):
    pass


class CSSPropertyAPIHeadersWriter(CSSPropertyAPIWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertyAPIHeadersWriter, self).__init__([json5_file_paths[0]])
        assert len(json5_file_paths) == 2,\
            ('CSSPropertyAPIHeadersWriter requires 2 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        # Map of API method name -> (return_type, parameters)
        self._api_methods = {}
        api_methods = json5_generator.Json5File.load_from_files(
            [json5_file_paths[1]])
        for api_method in api_methods.name_dictionaries:
            self._api_methods[api_method['name']] = ApiMethod(
                name=api_method['name'],
                return_type=api_method['return_type'],
                parameters=api_method['parameters'],
            )

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

if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIHeadersWriter).main()
