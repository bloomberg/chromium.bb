#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import sys

import json5_generator
import template_expander
import make_style_builder

from name_utilities import (
    enum_for_css_keyword, enum_type_name, enum_value_name, class_member_name, method_name
)


# Temporary hard-coded list of fields that are not CSS properties.
# TODO(shend): Put this into its own JSON5 file.
NONPROPERTIES = [
    {'name': 'IsLink', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    {'name': 'InsideLink', 'field_template': 'keyword', 'initial_keyword': 'not-inside-link',
     'keywords': ['not-inside-link', 'inside-unvisited-link', 'inside-visited-link'],
     'inherited': True, 'independent': False},
    # Style can not be shared.
    {'name': 'Unique', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    # Whether this style is affected by these pseudo-classes.
    {'name': 'AffectedByFocus', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    {'name': 'AffectedByHover', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    {'name': 'AffectedByActive', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    {'name': 'AffectedByDrag', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    # A non-inherited property references a variable or @apply is used
    {'name': 'HasVariableReferenceFromNonInheritedProperty', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    # Explicitly inherits a non-inherited property
    {'name': 'HasExplicitlyInheritedProperties', 'field_template': 'monotonic_flag',
     'inherited': False, 'independent': False},
    # These properties only have generated storage, and their methods are handwritten in ComputedStyle.
    # TODO(shend): Remove these fields and delete the 'storage_only' template.
    {'name': 'EmptyState', 'field_template': 'storage_only', 'size': 1, 'default_value': 'false',
     'type_name': 'bool', 'inherited': False, 'independent': False},
    {'name': 'StyleType', 'field_template': 'storage_only', 'size': 6, 'default_value': '0',
     'type_name': 'PseudoId', 'inherited': False, 'independent': False},
    {'name': 'PseudoBits', 'field_template': 'storage_only', 'size': 8, 'default_value': 'PseudoIdNone',
     'type_name': 'PseudoId', 'inherited': False, 'independent': False},
    # True if 'underline solid' is the only text decoration on this element.
    {'name': 'HasSimpleUnderline', 'field_template': 'storage_only', 'size': 1, 'default_value': 'false',
     'type_name': 'bool', 'inherited': True, 'independent': False},
]


class Field(object):
    """
    The generated ComputedStyle object is made up of a series of Fields.
    Each Field has a name, size, type, etc, and a bunch of attributes to
    determine which methods it will be used in.

    A Field also has enough information to use any storage type in C++, such as
    regular member variables, or more complex storage like vectors or hashmaps.
    Almost all properties will have at least one Field, often more than one.

    Most attributes in this class correspond to parameters in CSSProperties.json5.
    See that file for a more detailed explanation of each attribute.

    Attributes:
        field_role: The semantic role of the field. Can be:
            - 'property': for fields that store CSS properties
            - 'inherited_flag': for single-bit flags that store whether a property is
                                inherited by this style or set explicitly
            - 'nonproperty': for fields that are not CSS properties
        name_for_methods: String used to form the names of getters and setters.
            Should be in upper camel case.
        property_name: Name of the property that the field is part of.
        type_name: Name of the C++ type exposed by the generated interface (e.g. EClear, int).
        field_template: Determines the interface generated for the field. Can be one of:
           keyword, flag, or monotonic_flag.
        size: Number of bits needed for storage.
        default_value: Default value for this field when it is first initialized.
    """

    def __init__(self, field_role, name_for_methods, property_name, type_name,
                 field_template, size, default_value, **kwargs):
        """Creates a new field."""
        self.name = class_member_name(name_for_methods)
        self.property_name = property_name
        self.type_name = type_name
        self.field_template = field_template
        self.size = size
        self.default_value = default_value

        # Field role: one of these must be true
        self.is_property = field_role == 'property'
        self.is_inherited_flag = field_role == 'inherited_flag'
        self.is_nonproperty = field_role == 'nonproperty'
        assert (self.is_property, self.is_inherited_flag, self.is_nonproperty).count(True) == 1, \
            'Field role has to be exactly one of: property, inherited_flag, nonproperty'

        if not self.is_inherited_flag:
            self.is_inherited = kwargs.pop('inherited')
            self.is_independent = kwargs.pop('independent')
            assert self.is_inherited or not self.is_independent, 'Only inherited fields can be independent'

            self.is_inherited_method_name = method_name(name_for_methods + 'IsInherited')

        # Method names
        getter_prefix = 'Get' if name_for_methods == self.type_name else ''
        self.getter_method_name = method_name(getter_prefix + name_for_methods)
        self.setter_method_name = method_name('Set' + name_for_methods)
        self.initial_method_name = method_name('Initial' + name_for_methods)
        self.resetter_method_name = method_name('Reset' + name_for_methods)

        assert len(kwargs) == 0, 'Unexpected arguments provided to Field: ' + str(kwargs)


def _get_include_paths(properties):
    """
    Get a list of paths that need to be included for ComputedStyleBase.
    """
    include_paths = set()
    for property_ in properties:
        if property_['field_type_path'] is not None:
            include_paths.add(property_['field_type_path'] + '.h')
    return list(sorted(include_paths))


def _create_enums(properties):
    """
    Returns a dictionary of enums to be generated, enum name -> [list of enum values]
    """
    enums = {}
    for property_ in properties:
        # Only generate enums for keyword properties that use the default field_type_path.
        if property_['field_template'] == 'keyword' and property_['field_type_path'] is None:
            enum_name = property_['type_name']
            enum_values = [enum_value_name(k) for k in property_['keywords']]

            if enum_name in enums:
                # There's an enum with the same name, check if the enum values are the same
                assert set(enums[enum_name]) == set(enum_values), \
                    ("'" + property_['name'] + "' can't have type_name '" + enum_name + "' "
                     "because it was used by a previous property, but with a different set of keywords. "
                     "Either give it a different name or ensure the keywords are the same.")

            enums[enum_name] = enum_values

    return enums


def _create_field(field_role, property_):
    """
    Create a property or nonproperty field.
    """
    assert field_role in ('property', 'nonproperty')

    name_for_methods = property_['name_for_methods']

    if property_['field_template'] == 'keyword':
        assert property_['initial_keyword'] is not None, \
            ('MakeComputedStyleBase requires an initial keyword for keyword fields, none specified '
             'for property ' + property_['name'])
        type_name = property_['type_name']
        default_value = type_name + '::' + enum_value_name(property_['initial_keyword'])
        size = int(math.ceil(math.log(len(property_['keywords']), 2)))
    elif property_['field_template'] == 'storage_only':
        # 'storage_only' fields need to specify a size, type_name and default_value
        type_name = property_['type_name']
        default_value = property_['default_value']
        size = property_['size']
    else:
        assert property_['field_template'] in ('flag', 'monotonic_flag')
        type_name = 'bool'
        default_value = 'false'
        size = 1

    return Field(
        field_role,
        name_for_methods,
        property_name=property_['name'],
        inherited=property_['inherited'],
        independent=property_['independent'],
        type_name=type_name,
        field_template=property_['field_template'],
        size=size,
        default_value=default_value,
    )


def _create_inherited_flag_field(property_):
    """
    Create the field used for an inheritance fast path from an independent CSS property,
    and return the Field object.
    """
    return Field(
        'inherited_flag',
        property_['name_for_methods'] + 'IsInherited',
        property_name=property_['name'],
        type_name='bool',
        field_template='flag',
        size=1,
        default_value='true',
    )


def _create_fields(field_role, properties):
    """
    Create ComputedStyle fields from properties or nonproperties and return a list of Field objects.
    """
    fields = []
    for property_ in properties:
        # Only generate properties that have a field template
        if property_['field_template'] is not None:
            # If the property is independent, add the single-bit sized isInherited flag
            # to the list of Fields as well.
            if property_['independent']:
                fields.append(_create_inherited_flag_field(property_))

            fields.append(_create_field(field_role, property_))

    return fields


def _pack_fields(fields):
    """
    Group a list of fields into buckets to minimise padding.
    Returns a list of buckets, where each bucket is a list of Field objects.
    """
    # Since fields cannot cross word boundaries, in order to minimize
    # padding, group fields into buckets so that as many buckets as possible
    # are exactly 32 bits. Although this greedy approach may not always
    # produce the optimal solution, we add a static_assert to the code to
    # ensure ComputedStyleBase results in the expected size. If that
    # static_assert fails, this code is falling into the small number of
    # cases that are suboptimal, and may need to be rethought.
    # For more details on packing bitfields to reduce padding, see:
    # http://www.catb.org/esr/structure-packing/#_bitfields
    field_buckets = []
    # Consider fields in descending order of size to reduce fragmentation
    # when they are selected.
    for field in sorted(fields, key=lambda f: f.size, reverse=True):
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

    return field_buckets


class ComputedStyleBaseWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, json5_file_path):
        super(ComputedStyleBaseWriter, self).__init__(json5_file_path)
        self._outputs = {
            'ComputedStyleBase.h': self.generate_base_computed_style_h,
            'ComputedStyleBase.cpp': self.generate_base_computed_style_cpp,
            'ComputedStyleBaseConstants.h': self.generate_base_computed_style_constants,
            'CSSValueIDMappingsGenerated.h': self.generate_css_value_mappings,
        }

        # TODO(shend): Remove this once we move NONPROPERTIES to its own JSON file,
        # since the JSON5 reader will handle missing fields and defaults.
        for property_ in NONPROPERTIES:
            property_['name_for_methods'] = property_['name']
            if 'field_type_path' not in property_:
                property_['field_type_path'] = None
            if 'type_name' not in property_:
                property_['type_name'] = 'E' + enum_type_name(property_['name_for_methods'])

        property_values = self._properties.values()

        # Override the type name when field_type_path is specified
        for property_ in property_values:
            if property_['field_type_path']:
                property_['type_name'] = property_['field_type_path'].split('/')[-1]

        self._generated_enums = _create_enums(property_values + NONPROPERTIES)

        all_fields = (_create_fields('property', property_values) +
                      _create_fields('nonproperty', NONPROPERTIES))

        # Group fields into buckets
        field_buckets = _pack_fields(all_fields)

        # The expected size of ComputedStyleBase is equivalent to as many words
        # as the total number of buckets.
        self._expected_total_field_bytes = len(field_buckets)

        # The most optimal size of ComputedStyleBase is the total sum of all the
        # field sizes, rounded up to the nearest word. If this produces the
        # incorrect value, either the packing algorithm is not optimal or there
        # is no way to pack the fields such that excess padding space is not
        # added.
        # If this fails, increase padding_bytes by 1, but be aware that
        # this also increases ComputedStyleBase by 1 word.
        # We should be able to bring padding_bytes back to 0 from time to
        # time.
        padding_bytes = 0
        optimal_total_field_bytes = int(math.ceil(sum(f.size for f in all_fields) / 32.0))
        real_total_field_bytes = optimal_total_field_bytes + padding_bytes
        assert self._expected_total_field_bytes == real_total_field_bytes, \
            ('The field packing algorithm produced %s bytes, optimal is %s bytes' %
             (self._expected_total_field_bytes, real_total_field_bytes))

        # Order the fields so fields in each bucket are adjacent.
        self._fields = []
        for bucket in field_buckets:
            for field in bucket:
                self._fields.append(field)

        self._include_paths = _get_include_paths(property_values + NONPROPERTIES)


    @template_expander.use_jinja('ComputedStyleBase.h.tmpl')
    def generate_base_computed_style_h(self):
        return {
            'properties': self._properties,
            'enums': self._generated_enums,
            'include_paths': self._include_paths,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

    @template_expander.use_jinja('ComputedStyleBase.cpp.tmpl')
    def generate_base_computed_style_cpp(self):
        return {
            'properties': self._properties,
            'enums': self._generated_enums,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

    @template_expander.use_jinja('ComputedStyleBaseConstants.h.tmpl')
    def generate_base_computed_style_constants(self):
        return {
            'properties': self._properties,
            'enums': self._generated_enums,
            'fields': self._fields,
            'expected_total_field_bytes': self._expected_total_field_bytes,
        }

    @template_expander.use_jinja('CSSValueIDMappingsGenerated.h.tmpl')
    def generate_css_value_mappings(self):
        mappings = {}

        for property_ in self._properties.values():
            if property_['field_template'] == 'keyword':
                mappings[property_['type_name']] = {
                    'default_value': enum_value_name(property_['initial_keyword']),
                    'mapping': [(enum_value_name(k), enum_for_css_keyword(k)) for k in property_['keywords']],
                }

        return {
            'include_paths': self._include_paths,
            'mappings': mappings,
        }

if __name__ == '__main__':
    json5_generator.Maker(ComputedStyleBaseWriter).main()
