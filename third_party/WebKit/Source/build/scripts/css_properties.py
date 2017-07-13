#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
from name_utilities import (
    upper_camel_case, lower_camel_case, enum_for_css_property, enum_for_css_property_alias
)


# Check properties parameters are valid.
# TODO(jiameng): add more flag checks later.
def check_property_parameters(property_to_check):
    # Only longhand properties can be interpolable.
    if property_to_check['longhands']:
        assert not(property_to_check['interpolable']), \
            'Shorthand property (' + property_to_check['name'] + ') ' \
            'cannot be interpolable'
    if property_to_check['api_class'] is not None:
        if property_to_check['longhands']:
            assert 'parseSingleValue' not in property_to_check['api_methods'], \
                'Shorthand property (' + property_to_check['name'] + ') ' \
                'should not implement parseSingleValue'
        else:
            assert 'parseShorthand' not in property_to_check['api_methods'], \
                'Longhand property (' + property_to_check['name'] + ') ' \
                'should not implement parseShorthand'



class CSSProperties(json5_generator.Writer):
    def __init__(self, file_paths):
        json5_generator.Writer.__init__(self, file_paths)

        # StylePropertyMetadata assumes that there are at most 1024 properties + aliases.
        self._alias_offset = 512
        # 0: CSSPropertyInvalid
        # 1: CSSPropertyApplyAtRule
        # 2: CSSPropertyVariable
        self._first_enum_value = 3

        properties = self.json5_file.name_dictionaries

        # Sort properties by priority, then alphabetically.
        for property in properties:
            check_property_parameters(property)
            # This order must match the order in CSSPropertyPriority.h.
            priority_numbers = {'Animation': 0, 'High': 1, 'Low': 2}
            priority = priority_numbers[property['priority']]
            name_without_leading_dash = property['name']
            if property['name'].startswith('-'):
                name_without_leading_dash = property['name'][1:]
            property['sorting_key'] = (priority, name_without_leading_dash)

        # Assert there are no key collisions.
        sorting_keys = [p['sorting_key'] for p in properties]
        assert len(sorting_keys) == len(set(sorting_keys)), \
            ('Collision detected - two properties have the same name and priority, '
             'a potentially non-deterministic ordering can occur.')
        properties.sort(key=lambda p: p['sorting_key'])

        self._aliases = [property for property in properties if property['alias_for']]
        properties = [property for property in properties if not property['alias_for']]

        assert (self._first_enum_value + len(properties) < self._alias_offset), (
            'Property aliasing expects there are under %d properties.' % self._alias_offset)

        for property in properties:
            assert property['is_descriptor'] or property['is_property'], \
                property['name'] + ' must be either a property, a descriptor' +\
                ' or both'

        for offset, property in enumerate(properties):
            property['property_id'] = enum_for_css_property(property['name'])
            property['upper_camel_name'] = upper_camel_case(property['name'])
            property['lower_camel_name'] = lower_camel_case(property['name'])
            property['enum_value'] = self._first_enum_value + offset
            property['is_internal'] = property['name'].startswith('-internal-')

        self._properties_including_aliases = properties
        self._properties = {property['property_id']: property for property in properties}

        # The generated code will only work with at most one alias per property.
        assert len({property['alias_for'] for property in self._aliases}) == len(self._aliases)

        # Check that alias_for is not being used with a runtime flag.
        for property_ in self._aliases:
            assert not property_['runtime_flag'], \
                ("Property '" + property_['name'] + "' is an alias with a runtime_flag, "
                 "but runtime flags do not currently work for aliases.")

        # Update property aliases to include the fields of the property being aliased.
        for i, alias in enumerate(self._aliases):
            aliased_property = self._properties[
                enum_for_css_property(alias['alias_for'])]
            updated_alias = aliased_property.copy()
            updated_alias['name'] = alias['name']
            updated_alias['alias_for'] = alias['alias_for']
            updated_alias['property_id'] = enum_for_css_property_alias(alias['name'])
            updated_alias['enum_value'] = aliased_property['enum_value'] + self._alias_offset
            updated_alias['upper_camel_name'] = upper_camel_case(alias['name'])
            updated_alias['lower_camel_name'] = lower_camel_case(alias['name'])
            self._aliases[i] = updated_alias
        self._properties_including_aliases += self._aliases

        self.last_unresolved_property_id = max(property["enum_value"] for property in self._properties_including_aliases)

    def properties(self):
        return self._properties
