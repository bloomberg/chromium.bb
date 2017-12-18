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

from collections import defaultdict, namedtuple
from make_css_property_base import CSSPropertyWriter


class PropertyMethod(namedtuple('PropertyMethod', 'name,return_type,parameters')):
    pass


class CSSPropertyHeadersWriter(CSSPropertyWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertyHeadersWriter, self).__init__(json5_file_paths)
        assert len(json5_file_paths) == 3,\
            ('CSSPropertyHeadersWriter requires 3 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        # Map of property method name -> (return_type, parameters)
        self._property_methods = {}
        property_methods = json5_generator.Json5File.load_from_files(
            [json5_file_paths[2]])
        for property_method in property_methods.name_dictionaries:
            self._property_methods[property_method['name']] = PropertyMethod(
                name=property_method['name'],
                return_type=property_method['return_type'],
                parameters=property_method['parameters'],
            )
        self.validate_input()

        self._outputs = {}
        output_dir = sys.argv[sys.argv.index('--output_dir') + 1]
        properties = self.css_properties.longhands
        namespace_group = 'Longhand'
        if 'shorthands' in output_dir:
            properties = self.css_properties.shorthands
            namespace_group = 'Shorthand'
        for property_ in properties:
            if property_['property_class'] is None:
                continue
            property_['unique'] = isinstance(
                property_['property_class'], types.BooleanType)
            property_['property_methods'] = [
                self._property_methods[method_name]
                for method_name in property_['property_methods']
            ]
            property_['namespace_group'] = namespace_group
            class_data = self.get_class(property_)
            self.calculate_apply_functions_to_declare(property_)
            self.populate_includes(property_)
            self._outputs[class_data.classname + '.h'] = (
                self.generate_property_h_builder(
                    class_data.classname, property_))
        for property_ in self.css_properties.aliases:
            if ('shorthands' in output_dir and property_['longhands']) or \
               ('longhands' in output_dir and not property_['longhands']):
                if property_['property_class'] is None:
                    continue
                property_['unique'] = True
                class_data = self.get_class(property_)
                property_['namespace_group'] = namespace_group
                self.populate_includes(property_)
                self._outputs[class_data.classname + '.h'] = (
                    self.generate_property_h_builder(
                        class_data.classname, property_))

    def generate_property_h_builder(self, property_classname, property_):
        @template_expander.use_jinja(
            'core/css/properties/templates/CSSPropertySubclass.h.tmpl')
        def generate_property_h():
            return {
                'input_files': self._input_files,
                'property_classname': property_classname,
                'property': property_,
            }
        return generate_property_h

    def calculate_apply_functions_to_declare(self, property_):
        # Functions should only be declared on the property classes if they are
        # implemented and not shared (denoted by property_class = true. Shared
        # classes are denoted by property_class = "some string").
        property_['should_declare_apply_functions'] = \
            property_['unique'] \
            and property_['is_property'] \
            and not property_['longhands'] \
            and not property_['direction_aware_options'] \
            and not property_['builder_skip']
        if property_['custom_apply_functions_all']:
            property_name = property_['upper_camel_name']
            if (property_name in
                    ['Clip', 'ColumnCount', 'ColumnWidth', 'ZIndex']):
                property_['custom_apply'] = "auto"
                property_['custom_apply_args'] = {'auto_identity': 'CSSValueAuto'}
            if property_name == 'ColumnGap':
                property_['custom_apply'] = "auto"
                property_['custom_apply_args'] = {
                    'auto_getter': 'HasNormalColumnGap',
                    'auto_setter': 'SetHasNormalColumnGap',
                    'auto_identity': 'CSSValueNormal'}
            if (property_name in
                    ['BorderImageOutset', 'BorderImageRepeat',
                     'BorderImageSlice', 'BorderImageWidth',
                     'WebkitMaskBoxImageOutset', 'WebkitMaskBoxImageRepeat',
                     'WebkitMaskBoxImageSlice', 'WebkitMaskBoxImageWidth']):
                property_['custom_apply'] = 'border_image'
                is_mask_box = 'WebkitMaskBox' in property_name
                getter = 'MaskBoxImage' if is_mask_box else 'BorderImage'
                modifier_type = property_name[len('WebkitMaskBoxImage'):] if is_mask_box else property_name[len('BorderImage'):]
                property_['custom_apply_args'] = {
                    'is_mask_box': is_mask_box,
                    'modifier_type': modifier_type,
                    'getter': getter,
                    'setter': 'Set' + getter
                }
        property_['should_implement_apply_functions'] = (
            property_['should_declare_apply_functions'] and
            (not (property_['custom_apply_functions_initial'] and
                  property_['custom_apply_functions_inherit'] and
                  property_['custom_apply_functions_value']) or
             'custom_apply' in property_.keys()))

    def populate_includes(self, property_):
        includes = []
        if property_['alias_for']:
            includes.append("core/css/properties/CSSUnresolvedProperty.h")
        else:
            includes.append("core/css/properties/" + property_['namespace_group'] + ".h")
            if property_['direction_aware_options']:
                includes.append("core/StylePropertyShorthand.h")
            if property_['runtime_flag']:
                includes.append("platform/runtime_enabled_features.h")
            if property_['should_implement_apply_functions']:
                includes.append("core/css/resolver/StyleResolverState.h")
                if property_['converter'] == "CSSPrimitiveValue":
                    includes.append("core/css/CSSPrimitiveValue.h")
                    includes.append("core/css/CSSPrimitiveValueMappings.h")
                elif property_['converter'] == "CSSIdentifierValue":
                    includes.append("core/css/CSSIdentifierValue.h")
                elif property_['converter']:
                    includes.append("core/css/CSSPrimitiveValueMappings.h")
                    includes.append("core/css/resolver/StyleBuilderConverter.h")
                if property_['font']:
                    includes.append("core/css/resolver/FontBuilder.h")
                elif property_['svg']:
                    includes.append("core/css/CSSPrimitiveValueMappings.h")
                    includes.append("core/style/ComputedStyle.h")
                    includes.append("core/style/SVGComputedStyle.h")
                else:
                    includes.append("core/style/ComputedStyle.h")
                if (property_.get('custom_apply_args') and
                        property_.get('custom_apply_args').get('modifier_type')
                        in ['Width', 'Slice', 'Outset']):
                    includes.append("core/css/properties/StyleBuildingUtils.h")
        includes.sort()
        property_['includes'] = includes

    def validate_input(self):
        # First collect which classes correspond to which properties.
        class_names_to_properties = defaultdict(list)
        for property_ in self.css_properties.properties_including_aliases:
            if property_['property_class'] is None:
                continue
            class_data = self.get_class(property_)
            class_names_to_properties[class_data.classname].append(property_)
        # Check that properties that share class names specify the same method
        # names.
        for properties in class_names_to_properties.values():
            if len(properties) == 1:
                assert properties[0]['property_class'] is True, 'Unique property_class ' \
                    '{} defined on {}. property_class as a string is reserved for ' \
                    'use with properties that share logic. Did you mean ' \
                    'property_class: true?'.format(
                        properties[0]['property_class'], properties[0]['name'])

            property_methods = set(properties[0]['property_methods'])
            for property_ in properties[1:]:
                assert property_methods == set(property_['property_methods']), \
                    'Properties {} and {} share the same property_class but do ' \
                    'not declare the same property_methods. Properties sharing ' \
                    'the same property_class must declare the same list of ' \
                    'property_methods to ensure deterministic code ' \
                    'generation.'.format(
                        properties[0]['name'], property_['name'])


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyHeadersWriter).main()
