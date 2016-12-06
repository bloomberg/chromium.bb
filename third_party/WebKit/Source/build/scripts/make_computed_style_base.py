#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import namedtuple
import math
import sys

import in_generator
import template_expander
import make_style_builder

from name_utilities import camel_case

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

        # A list of fields for the generated class.
        # TODO(sashab): Rename this to Member, and add better documentation for what this list is for,
        # including asserts to show inter-field dependencies.
        Field = namedtuple('Field', [
            # Name of field member variable
            'name',
            # Property field is for
            'property',
            # Field family: one of these must be true
            'is_enum',
            'is_inherited_flag',
            # Field storage type
            'type',
            # Bits needed for storage
            'size',
            # Default value for field
            'default_value',
            # Method names
            'getter_method_name',
            'setter_method_name',
            'initial_method_name',
            'resetter_method_name',
            'is_inherited_method_name',
        ])

        # A list of all the fields to be generated. Add new fields here.
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
                # to the list of member variable as well.
                if property['independent']:
                    field_name_suffix_upper = property['upper_camel_name'] + 'IsInherited'
                    field_name_suffix_lower = property_name_lower + 'IsInherited'
                    self._fields.append(Field(
                        name='m_' + field_name_suffix_lower,
                        property=property,
                        is_enum=False,
                        is_inherited_flag=True,
                        type='bool',
                        size=1,
                        default_value='true',
                        getter_method_name=field_name_suffix_lower,
                        setter_method_name='set' + field_name_suffix_upper,
                        initial_method_name='initial' + field_name_suffix_upper,
                        resetter_method_name='reset' + field_name_suffix_upper,
                        is_inherited_method_name='',
                    ))

                # Add the property itself as a member variable.
                self._fields.append(Field(
                    name=field_name,
                    property=property,
                    is_enum=True,
                    is_inherited_flag=False,
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
