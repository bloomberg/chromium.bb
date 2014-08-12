#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import in_generator
from name_utilities import lower_first


class CSSProperties(in_generator.Writer):
    defaults = {
        'longhands': '',
        'font': False,
        'svg': False,
        'name_for_methods': None,
        'use_handlers_for': None,
        'getter': None,
        'setter': None,
        'initial': None,
        'type_name': None,
        'converter': None,
        'custom_all': False,
        'custom_initial': False,
        'custom_inherit': False,
        'custom_value': False,
        'builder_skip': False,
        'direction_aware': False,
    }

    valid_values = {
        'font': (True, False),
        'svg': (True, False),
        'custom_all': (True, False),
        'custom_initial': (True, False),
        'custom_inherit': (True, False),
        'custom_value': (True, False),
        'builder_skip': (True, False),
        'direction_aware': (True, False),
    }

    def __init__(self, file_paths):
        in_generator.Writer.__init__(self, file_paths)

        properties = self.in_file.name_dictionaries

        for property in properties:
            property['property_id'] = css_name_to_enum(property['name'])
            property['upper_camel_name'] = camelcase_css_name(property['name'])
            property['lower_camel_name'] = lower_first(property['upper_camel_name'])

        self._properties_list = properties
        self._properties = {property['property_id']: property for property in properties}


def camelcase_css_name(css_name):
    """Convert hyphen-separated-name to UpperCamelCase.

    E.g., '-foo-bar' becomes 'FooBar'.
    """
    return ''.join(word.capitalize() for word in css_name.split('-'))


def css_name_to_enum(css_name):
    return 'CSSProperty' + camelcase_css_name(css_name)
