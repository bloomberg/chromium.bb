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
from name_utilities import upper_camel_case


class ApiMethod(namedtuple('ApiMethod', 'name,return_type,parameters')):
    pass


def apply_computed_style_builder_function_parameters(property_):
    """Generate function parameters for generated implementations of Apply*
    """
    def set_if_none(property_, key, value):
        if property_[key] is None:
            property_[key] = value

    # These values are converted using CSSPrimitiveValue in the setter function,
    # if applicable.
    primitive_types = [
        'short',
        'unsigned short',
        'int',
        'unsigned int',
        'unsigned',
        'float',
        'LineClampValue'
    ]

    # Functions should only be declared on the API classes if they are
    # implemented and not shared (denoted by api_class = true. Shared classes
    # are denoted by api_class = "some string").
    property_['should_declare_application_functions'] = \
        property_['api_class'] \
        and isinstance(property_['api_class'], types.BooleanType) \
        and property_['is_property'] \
        and not property_['use_handlers_for'] \
        and not property_['longhands'] \
        and not property_['direction_aware'] \
        and not property_['builder_skip']
    if not property_['should_declare_application_functions']:
        return

    if property_['custom_all']:
        property_['custom_initial'] = True
        property_['custom_inherit'] = True
        property_['custom_value'] = True

    name = property_['name_for_methods']
    if not name:
        name = upper_camel_case(property_['name']).replace('Webkit', '')
    simple_type_name = str(property_['type_name']).split('::')[-1]
    set_if_none(property_, 'name_for_methods', name)

    set_if_none(property_, 'type_name', 'E' + name)
    set_if_none(property_, 'setter', 'Set' + name)
    if property_['type_name'] in primitive_types:
        set_if_none(property_, 'converter', 'CSSPrimitiveValue')
    else:
        set_if_none(property_, 'converter', 'CSSIdentifierValue')

    set_if_none(
        property_, 'getter', name if simple_type_name != name else 'Get' + name)
    set_if_none(property_, 'inherited', False)
    set_if_none(property_, 'initial', 'Initial' + name)

    if property_['inherited']:
        property_['is_inherited_setter'] = 'Set' + name + 'IsInherited'


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
            apply_computed_style_builder_function_parameters(property_)
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
