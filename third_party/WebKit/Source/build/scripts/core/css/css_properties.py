#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
from name_utilities import (
    upper_camel_case,
    lower_camel_case,
    enum_value_name,
    enum_for_css_property,
    enum_for_css_property_alias
)
from core.css.field_alias_expander import FieldAliasExpander


# These values are converted using CSSPrimitiveValue in the setter function,
# if applicable.
PRIMITIVE_TYPES = [
    'short',
    'unsigned short',
    'int',
    'unsigned int',
    'unsigned',
    'float',
    'LineClampValue'
]


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
        json5_generator.Writer.__init__(self, [file_paths[0]])

        # StylePropertyMetadata assumes that there are at most 1024 properties
        # + aliases.
        self._alias_offset = 512
        # 0: CSSPropertyInvalid
        # 1: CSSPropertyApplyAtRule
        # 2: CSSPropertyVariable
        self._first_enum_value = 3

        properties = self.json5_file.name_dictionaries

        # Sort properties by priority, then alphabetically.
        for property_ in properties:
            check_property_parameters(property_)
            # This order must match the order in CSSPropertyPriority.h.
            priority_numbers = {'Animation': 0, 'High': 1, 'Low': 2}
            priority = priority_numbers[property_['priority']]
            name_without_leading_dash = property_['name']
            if property_['name'].startswith('-'):
                name_without_leading_dash = property_['name'][1:]
            property_['sorting_key'] = (priority, name_without_leading_dash)

        # Assert there are no key collisions.
        sorting_keys = {}
        for property_ in properties:
            key = property_['sorting_key']
            assert key not in sorting_keys, \
                ('Collision detected - two properties have the same name and '
                 'priority, a potentially non-deterministic ordering can '
                 'occur: {}'.format(key))
            sorting_keys[key] = property_['name']
        properties.sort(key=lambda p: p['sorting_key'])

        self._aliases = [property_ for property_ in properties if property_['alias_for']]
        properties = [property_ for property_ in properties if not property_['alias_for']]

        assert (self._first_enum_value + len(properties) < self._alias_offset), (
            'Property aliasing expects there are under %d properties.' % self._alias_offset)

        self._field_alias_expander = FieldAliasExpander(file_paths[1])
        for offset, property_ in enumerate(properties):
            assert property_['is_descriptor'] or property_['is_property'], \
                property_['name'] + ' must be either a property, a descriptor' +\
                ' or both'
            property_['enum_value'] = self._first_enum_value + offset
            self.expand_parameters(property_)

        self._properties_including_aliases = properties
        self._properties = {property_['property_id']: property_ for property_ in properties}

        # The generated code will only work with at most one alias per property.
        assert len({property_['alias_for'] for property_ in self._aliases}) == len(self._aliases)

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

        self.last_unresolved_property_id = max(property_["enum_value"] for property_ in self._properties_including_aliases)

    def properties(self):
        return self._properties

    def expand_parameters(self, property_):
        def set_if_none(property_, key, value):
            if property_[key] is None:
                property_[key] = value

        # Basic info.
        property_['property_id'] = enum_for_css_property(property_['name'])
        property_['upper_camel_name'] = upper_camel_case(property_['name'])
        property_['lower_camel_name'] = lower_camel_case(property_['name'])
        property_['is_internal'] = property_['name'].startswith('-internal-')
        name = property_['name_for_methods']
        if not name:
            name = upper_camel_case(property_['name']).replace('Webkit', '')
        set_if_none(property_, 'inherited', False)

        # Initial function, Getters and Setters for ComputedStyle.
        set_if_none(property_, 'initial', 'Initial' + name)
        simple_type_name = str(property_['type_name']).split('::')[-1]
        set_if_none(property_, 'name_for_methods', name)
        set_if_none(property_, 'type_name', 'E' + name)
        set_if_none(
            property_,
            'getter',
            name if simple_type_name != name else 'Get' + name)
        set_if_none(property_, 'setter', 'Set' + name)
        if property_['inherited']:
            property_['is_inherited_setter'] = 'Set' + name + 'IsInherited'

        # Expand whether there are custom StyleBuilder methods.
        if property_['api_custom_apply_functions_all']:
            property_['api_custom_apply_functions_inherit'] = True
            property_['api_custom_apply_functions_initial'] = True
            property_['api_custom_apply_functions_value'] = True

        # Expand StyleBuilderConverter params where ncessary.
        if property_['type_name'] in PRIMITIVE_TYPES:
            set_if_none(property_, 'converter', 'CSSPrimitiveValue')
        else:
            set_if_none(property_, 'converter', 'CSSIdentifierValue')

        # Expand out field templates.
        if not property_['field_template']:
            return

        self._field_alias_expander.expand_field_alias(property_)

        type_name = property_['type_name']
        if (property_['field_template'] == 'keyword' or
                property_['field_template'] == 'multi_keyword'):
            default_value = type_name + '::' + \
                enum_value_name(property_['default_value'])
        elif (property_['field_template'] == 'external' or
              property_['field_template'] == 'primitive' or
              property_['field_template'] == 'pointer'):
            default_value = property_['default_value']
        else:
            assert property_['field_template'] == 'monotonic_flag', \
                "Please put a valid value for field_template; got " + \
                str(property_['field_template'])
            property_['type_name'] = 'bool'
            default_value = 'false'
        property_['default_value'] = default_value

        if property_['wrapper_pointer_name']:
            assert property_['field_template'] in ['pointer', 'external']
            if property_['field_template'] == 'external':
                property_['type_name'] = '{}<{}>'.format(
                    property_['wrapper_pointer_name'], type_name)
