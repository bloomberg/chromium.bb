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


class CSSPropertyAPIWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths):
        super(CSSPropertyAPIWriter, self).__init__([])
        self._input_files = json5_file_paths
        self._outputs = {
            'CSSPropertyAPI.h': self.generate_property_api_header,
            'CSSPropertyAPI.cpp': self.generate_property_api_implementation,
        }

        self._css_properties = css_properties.CSSProperties(json5_file_paths)

        # A list of (enum_value, property_id, api_classname) tuples.
        self._api_classes_by_property_id = []
        # Just a set of class names.
        self._shorthand_api_classes = set()
        self._longhand_api_classes = set()
        for property_ in self._css_properties.longhands:
            api_class = self.get_class(property_, 'CSSPropertyAPI')
            self._api_classes_by_property_id.append(api_class)
            self._longhand_api_classes.add(api_class.classname)
        for property_ in self._css_properties.shorthands:
            api_class = self.get_class(property_, 'CSSShorthandPropertyAPI')
            self._api_classes_by_property_id.append(api_class)
            self._shorthand_api_classes.add(api_class.classname)

        # Manually add CSSPropertyVariable and CSSPropertyAtApply.
        self._api_classes_by_property_id.append(
            ApiClassData(
                enum_value=1,
                property_id="CSSPropertyApplyAtRule",
                classname="CSSPropertyAPIApplyAtRule"))
        self._longhand_api_classes.add("CSSPropertyAPIApplyAtRule")
        self._api_classes_by_property_id.append(
            ApiClassData(
                enum_value=2,
                property_id="CSSPropertyVariable",
                classname="CSSPropertyAPIVariable"))
        self._longhand_api_classes.add("CSSPropertyAPIVariable")

        # Sort by enum value.
        self._api_classes_by_property_id.sort(key=lambda t: t.enum_value)

    def get_class(self, property_, prefix=''):
        """Gets the classname for a given property.

        If the property has api_class set to True, returns an automatically
        generated class name. If it is set to a string, returns that. If it is
        set to None, returns the CSSPropertyAPI base class.

        Args:
            property_: A single property from CSSProperties.properties()
        Returns:
            The name to use for the API, or None.
        """
        assert property_['api_class'] is True or \
            property_['api_class'] is None or \
            isinstance(property_['api_class'], str), \
            "api_class value for {} should be None, True or a string".format(
                property_['name'])
        classname = property_['api_class']
        if property_['api_class'] is None:
            classname = 'CSSPropertyAPI'
        if property_['api_class'] is True:
            classname = prefix + property_['upper_camel_name']
        return ApiClassData(
            enum_value=property_['enum_value'],
            property_id=property_['property_id'],
            classname=classname)

    @property
    def css_properties(self):
        return self._css_properties

    @template_expander.use_jinja(
        'core/css/properties/templates/CSSPropertyAPI.h.tmpl')
    def generate_property_api_header(self):
        return {
            'input_files': self._input_files,
            'api_classes_by_property_id': self._api_classes_by_property_id,
        }

    @template_expander.use_jinja(
        'core/css/properties/templates/CSSPropertyAPI.cpp.tmpl')
    def generate_property_api_implementation(self):
        return {
            'input_files': self._input_files,
            'longhand_api_classnames': self._longhand_api_classes,
            'shorthand_api_classnames': self._shorthand_api_classes,
            'api_classes_by_property_id': self._api_classes_by_property_id,
            'last_property_id': self._css_properties.last_property_id
        }


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIWriter).main()
