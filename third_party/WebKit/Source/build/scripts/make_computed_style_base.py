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
        # Internal field storage type (storage_type_path can be None)
        self.storage_type = kwargs.pop('storage_type')
        self.storage_type_path = kwargs.pop('storage_type_path')
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
            # Only generate enums for keyword properties that use the default field_storage_type.
            if property['keyword_only'] and property['field_storage_type'] is None:
                enum_name = property['type_name']
                # From the Blink style guide: Enum members should use InterCaps with an initial capital letter. [names-enum-members]
                enum_values = [camel_case(k) for k in property['keywords']]
                self._computed_enums[enum_name] = enum_values

        # A list of all the fields to be generated.
        all_fields = []
        for property in self._properties.values():
            if property['keyword_only']:
                property_name = property['name_for_methods']
                property_name_lower = property_name[0].lower() + property_name[1:]

                # From the Blink style guide: Other data members should be prefixed by "m_". [names-data-members]
                field_name = 'm_' + property_name_lower
                bits_needed = math.log(len(property['keywords']), 2)

                # Separate the type path from the type name, if specified.
                type_name = property['type_name']
                type_path = None
                if property['field_storage_type']:
                    type_path = property['field_storage_type']
                    type_name = type_path.split('/')[-1]

                # For now, the getter name should match the field name. Later, getter names
                # will start with an uppercase letter, so if they conflict with the type name,
                # add 'get' to the front.
                getter_method_name = property_name_lower
                if type_name == property_name:
                    getter_method_name = 'get' + property_name

                assert property['initial_keyword'] is not None, \
                    ('MakeComputedStyleBase requires an initial keyword for keyword_only values, none specified '
                     'for property ' + property['name'])
                default_value = type_name + '::' + camel_case(property['initial_keyword'])

                # If the property is independent, add the single-bit sized isInherited flag
                # to the list of Fields as well.
                if property['independent']:
                    field_name_suffix_upper = property_name + 'IsInherited'
                    field_name_suffix_lower = property_name_lower + 'IsInherited'
                    all_fields.append(Field(
                        'inherited_flag',
                        name='m_' + field_name_suffix_lower,
                        property_name=property['name'],
                        storage_type='bool',
                        storage_type_path=None,
                        size=1,
                        default_value='true',
                        getter_method_name=field_name_suffix_lower,
                        setter_method_name='set' + field_name_suffix_upper,
                        initial_method_name='initial' + field_name_suffix_upper,
                        resetter_method_name='reset' + field_name_suffix_upper,
                    ))

                # Add the property itself as a member variable.
                all_fields.append(Field(
                    'enum',
                    name=field_name,
                    property_name=property['name'],
                    inherited=property['inherited'],
                    independent=property['independent'],
                    storage_type=type_name,
                    storage_type_path=type_path,
                    size=int(math.ceil(bits_needed)),
                    default_value=default_value,
                    getter_method_name=getter_method_name,
                    setter_method_name='set' + property_name,
                    initial_method_name='initial' + property_name,
                    resetter_method_name='reset' + property_name,
                    is_inherited_method_name=property_name_lower + 'IsInherited',
                ))

        # Since fields cannot cross word boundaries, in order to minimize
        # padding, group fields into buckets so that as many buckets as possible
        # that are exactly 32 bits. Although this greedy approach may not always
        # produce the optimal solution, we add a static_assert to the code to
        # ensure ComputedStyleBase results in the expected size. If that
        # static_assert fails, this code is falling into the small number of
        # cases that are suboptimal, and may need to be rethought.
        # For more details on packing bitfields to reduce padding, see:
        # http://www.catb.org/esr/structure-packing/#_bitfields
        field_buckets = []
        # Consider fields in descending order of size to reduce fragmentation
        # when they are selected.
        for field in sorted(all_fields, key=lambda f: f.size, reverse=True):
            added_to_bucket = False
            # Go through each bucket and add this field if it will not increase
            # the bucket's size to larger than 32 bits. Otherwise, make a new
            # bucket containing only this field.
            for bucket in field_buckets:
                if sum(f.size for f in bucket) + field.size <= 32:
                    bucket.append(field)
                    added_to_bucket = True
                    break
            if not added_to_bucket:
                field_buckets.append([field])

        # The expected size of ComputedStyleBase is equivalent to as many words
        # as the total number of buckets.
        self._expected_total_field_bytes = len(field_buckets)

        # The most optimal size of ComputedStyleBase is the total sum of all the
        # field sizes, rounded up to the nearest word. If this produces the
        # incorrect value, either the packing algorithm is not optimal or there
        # is no way to pack the fields such that excess padding space is not
        # added.
        # If this fails, increase extra_padding_bytes by 1, but be aware that
        # this also increases ComputedStyleBase by 1 word.
        # We should be able to bring extra_padding_bytes back to 0 from time to
        # time.
        extra_padding_bytes = 0
        optimal_total_field_bytes = int(math.ceil(sum(f.size for f in all_fields) / 32.0))
        real_total_field_bytes = optimal_total_field_bytes + extra_padding_bytes
        assert self._expected_total_field_bytes == real_total_field_bytes, \
            ('The field packing algorithm produced %s bytes, optimal is %s bytes' %
             (len(field_buckets), self._expected_total_field_bytes))

        # Order the fields so fields in each bucket are adjacent.
        self._fields = []
        for bucket in field_buckets:
            for field in bucket:
                self._fields.append(field)

    @template_expander.use_jinja('ComputedStyleBase.h.tmpl')
    def generate_base_computed_style_h(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

    @template_expander.use_jinja('ComputedStyleBase.cpp.tmpl')
    def generate_base_computed_style_cpp(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

    @template_expander.use_jinja('ComputedStyleBaseConstants.h.tmpl')
    def generate_base_computed_style_constants(self):
        return {
            'properties': self._properties,
            'enums': self._computed_enums,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

if __name__ == '__main__':
    in_generator.Maker(ComputedStyleBaseWriter).main(sys.argv)
