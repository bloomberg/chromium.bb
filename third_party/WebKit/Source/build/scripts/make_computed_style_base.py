#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import sys

import in_generator
import template_expander
import make_style_builder

from name_utilities import camel_case


class Field(object):
    """
    The generated ComputedStyle object is made up of a series of Fields.
    Each Field has a name, size, type, etc, and a bunch of attributes to
    determine which methods it will be used in.

    A Field also has enough information to use any storage type in C++, such as
    regular member variables, or more complex storage like vectors or hashmaps.
    Almost all properties will have at least one Field, often more than one.

    Fields also fall into various families, which determine the logic that is
    used to generate them. The available field families are:
      - 'enum', for fields that store the values of an enum
      - 'inherited_flag', for single-bit flags that store whether a property is
                          inherited by this style or set explicitly
    """
    def __init__(self, field_family, **kwargs):
        # Values common to all fields
        # Name of field
        self.name = kwargs.pop('name')
        # Name of property field is for
        self.property_name = kwargs.pop('property_name')
        # Field storage type
        self.type = kwargs.pop('type')
        # Bits needed for storage
        self.size = kwargs.pop('size')
        # Default value for field
        self.default_value = kwargs.pop('default_value')
        # Method names
        self.getter_method_name = kwargs.pop('getter_method_name')
        self.setter_method_name = kwargs.pop('setter_method_name')
        self.initial_method_name = kwargs.pop('initial_method_name')
        self.resetter_method_name = kwargs.pop('resetter_method_name')

        # Field family: one of these must be true
        self.is_enum = field_family == 'enum'
        self.is_inherited_flag = field_family == 'inherited_flag'
        assert not (self.is_enum and self.is_inherited_flag), 'Only one field family can be specified at a time'

        if self.is_enum:
            # Enum-only fields
            self.is_inherited = kwargs.pop('inherited')
            self.is_independent = kwargs.pop('independent')
            assert self.is_inherited or not self.is_independent, 'Only inherited fields can be independent'

            self.is_inherited_method_name = kwargs.pop('is_inherited_method_name')
        elif self.is_inherited_flag:
            # Inherited flag-only fields
            pass

        assert len(kwargs) == 0, 'Unexpected arguments provided to Field: ' + str(kwargs)


class ComputedStyleBaseWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, in_file_path):
        super(ComputedStyleBaseWriter, self).__init__(in_file_path)
        self._outputs = {
            'ComputedStyleBase.h': self.generate_base_computed_style_h,
            'ComputedStyleBase.cpp': self.generate_base_computed_style_cpp,
            'ComputedStyleBaseConstants.h': self.generate_base_computed_style_constants,
        }

        # A map of enum name -> list of enum values
        self._computed_enums = {}
        for property in self._properties.values():
            if property['keyword_only']:
                enum_name = property['type_name']
                # From the Blink style guide: Enum members should use InterCaps with an initial capital letter. [names-enum-members]
                enum_values = [camel_case(k) for k in property['keywords']]
                self._computed_enums[enum_name] = enum_values

        # A list of all the fields to be generated.
        self._fields = []
        for property in self._properties.values():
            if property['keyword_only']:
                property_name = property['upper_camel_name']
                if property['name_for_methods']:
                    property_name = property['name_for_methods']
                property_name_lower = property_name[0].lower() + property_name[1:]

                # From the Blink style guide: Other data members should be prefixed by "m_". [names-data-members]
                field_name = 'm_' + property_name_lower
                bits_needed = math.log(len(property['keywords']), 2)
                type_name = property['type_name']

                assert property['initial_keyword'] is not None, \
                    ('MakeComputedStyleBase requires an initial keyword for keyword_only values, none specified '
                     'for property ' + property['name'])
                default_value = type_name + '::' + camel_case(property['initial_keyword'])

                # If the property is independent, add the single-bit sized isInherited flag
                # to the list of Fields as well.
                if property['independent']:
                    field_name_suffix_upper = property['upper_camel_name'] + 'IsInherited'
                    field_name_suffix_lower = property_name_lower + 'IsInherited'
                    self._fields.append(Field(
                        'inherited_flag',
                        name='m_' + field_name_suffix_lower,
                        property_name=property['name'],
                        type='bool',
                        size=1,
                        default_value='true',
                        getter_method_name=field_name_suffix_lower,
                        setter_method_name='set' + field_name_suffix_upper,
                        initial_method_name='initial' + field_name_suffix_upper,
                        resetter_method_name='reset' + field_name_suffix_upper,
                    ))

                # Add the property itself as a member variable.
                self._fields.append(Field(
                    'enum',
                    name=field_name,
                    property_name=property['name'],
                    inherited=property['inherited'],
                    independent=property['independent'],
                    type=type_name,
                    size=int(math.ceil(bits_needed)),
                    default_value=default_value,
                    getter_method_name=property_name_lower,
                    setter_method_name='set' + property_name,
                    initial_method_name='initial' + property_name,
                    resetter_method_name='reset' + property_name,
                    is_inherited_method_name=property_name_lower + 'IsInherited',
                ))

    @template_expander.use_jinja('ComputedStyleBase.h.tmpl')
    def generate_base_computed_style_h(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

    @template_expander.use_jinja('ComputedStyleBase.cpp.tmpl')
    def generate_base_computed_style_cpp(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

    @template_expander.use_jinja('ComputedStyleBaseConstants.h.tmpl')
    def generate_base_computed_style_constants(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
        }

if __name__ == '__main__':
    in_generator.Maker(ComputedStyleBaseWriter).main(sys.argv)
