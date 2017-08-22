#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

import json5_generator
import template_expander

from collections import namedtuple
from core.css import css_properties


class ApiClassData(
        namedtuple('ApiClassData', 'enum_value,property_id,classname')):
    pass


class CSSPropertyAPIWriter(css_properties.CSSProperties):
    def __init__(self, json5_file_paths):
        super(CSSPropertyAPIWriter, self).__init__([json5_file_paths[0]])
        self._outputs = {
            'CSSPropertyAPI.cpp': self.generate_property_api_baseclass,
        }

        # A list of (enum_value, property_id, api_class_name) tuples.
        self._api_classes_by_property_id = []
        # Just a set of class names.
        self._api_classes = set()
        for property_ in self.properties().values():
            api_class = self.get_classname(property_)
            if api_class is None:
                # Use the baseclass if no class was specified.
                api_class = "CSSPropertyAPI"
            self._api_classes_by_property_id.append(
                ApiClassData(
                    enum_value=property_['enum_value'],
                    property_id=property_['property_id'],
                    classname=api_class))
            self._api_classes.add(api_class)
        # Sort by enum value.
        self._api_classes_by_property_id.sort(key=lambda t: t.enum_value)

    def get_classname(self, property_):
        """Gets the classname for a given property.

        If the property has api_class set to True, returns an automatically
        generated class name. If it is set to a string, returns that. If it is
        set to None, returns None.

        Args:
            property_: A single property from CSSProperties.properties()
        Returns:
            The name to use for the API, or None.
        """
        if property_['api_class'] is None:
            return
        if property_['api_class'] is True:
            if property_['longhands']:
                api_prefix = 'CSSShorthandPropertyAPI'
            else:
                api_prefix = 'CSSPropertyAPI'
            return api_prefix + property_['upper_camel_name']
        # This property has a specified class name.
        assert isinstance(property_['api_class'], str), \
            ("api_class value for " + property_['api_class'] +
             " should be None, True or a string")
        return property_['api_class']

    @template_expander.use_jinja(
        'core/css/properties/templates/CSSPropertyAPI.cpp.tmpl')
    def generate_property_api_baseclass(self):
        return {
            'input_files': self._input_files,
            'api_classnames': self._api_classes,
            'api_classes_by_property_id': self._api_classes_by_property_id,
            'first_property_id': self._first_enum_value,
            'last_property_id': (self._first_enum_value +
                                 len(self._properties) - 1)
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIWriter).main()
