#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

import json5_generator
import template_expander

from collections import namedtuple
from make_css_property_base import CSSPropertyBaseWriter


class PropertyMethod(namedtuple('PropertyMethod', 'name,return_type,parameters')):
    pass


class CSSPropertiesWriter(CSSPropertyBaseWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertiesWriter, self).__init__(json5_file_paths)
        assert len(json5_file_paths) == 3,\
            ('CSSPropertiesWriter requires 3 input json5 files, ' +
             'got {}.'.format(len(json5_file_paths)))

        self.template_cache = {}

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

        self._outputs = {}
        output_dir = sys.argv[sys.argv.index('--output_dir') + 1]
        properties = self.css_properties.longhands
        namespace_group = 'Longhand'
        if 'shorthands' in output_dir:
            properties = self.css_properties.shorthands
            namespace_group = 'Shorthand'
        for property_ in properties:
            property_['property_methods'] = [
                self._property_methods[method_name]
                for method_name in property_['property_methods']
            ]
            property_['namespace_group'] = namespace_group
            class_data = self.get_class(property_)
            self.calculate_apply_functions_to_declare(property_)
            self._outputs[class_data.filename + '.h'] = (
                self.generate_property_h_builder(
                    class_data.classname, class_data.filename, property_))
            if 'should_implement_apply_functions_in_cpp' in property_:
                self._outputs[class_data.filename + '.cc'] = (
                    self.generate_property_cpp_builder(
                        class_data.filename, property_))
        for property_ in self.css_properties.aliases:
            if ('shorthands' in output_dir and property_['longhands']) or \
               ('longhands' in output_dir and not property_['longhands']):
                class_data = self.get_class(property_)
                property_['namespace_group'] = namespace_group
                self._outputs[class_data.filename + '.h'] = (
                    self.generate_property_h_builder(
                        class_data.classname, class_data.filename, property_))

    def generate_property_h_builder(self, property_classname, property_filename, property_):
        @template_expander.use_jinja(
            'core/css/properties/templates/css_property_subclass.h.tmpl',
            template_cache=self.template_cache)
        def generate_property_h():
            return {
                'input_files': self._input_files,
                'property_classname': property_classname,
                'property_filename': property_filename,
                'property': property_,
                'includes': sorted(list(self.h_includes(property_)))
            }
        return generate_property_h

    def generate_property_cpp_builder(self, property_filename, property_):
        @template_expander.use_jinja(
            'core/css/properties/templates/css_property_subclass.cc.tmpl',
            template_cache=self.template_cache)
        def generate_property_cpp():
            return {
                'input_files': self._input_files,
                'property_filename': property_filename,
                'property': property_,
                'includes': sorted(list(self.cpp_includes(property_)))
            }
        return generate_property_cpp

    def calculate_apply_functions_to_declare(self, property_):
        if property_['custom_apply_functions_all']:
            property_name = property_['upper_camel_name']
            if (property_name in ['Clip', 'ColumnCount', 'ColumnWidth', 'ZIndex']):
                property_['custom_apply'] = "auto"
                property_['custom_apply_args'] = {'auto_identity': 'CSSValueAuto'}
            elif (property_name in [
                    'BorderImageOutset', 'BorderImageRepeat', 'BorderImageSlice', 'BorderImageWidth', 'WebkitMaskBoxImageOutset',
                    'WebkitMaskBoxImageRepeat', 'WebkitMaskBoxImageSlice', 'WebkitMaskBoxImageWidth']):
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
            elif (property_name in [
                    'BackgroundAttachment', 'BackgroundBlendMode', 'BackgroundClip', 'BackgroundImage', 'BackgroundOrigin',
                    'BackgroundPositionX', 'BackgroundPositionY', 'BackgroundRepeatX', 'BackgroundRepeatY', 'BackgroundSize',
                    'MaskSourceType', 'WebkitMaskClip', 'WebkitMaskComposite', 'WebkitMaskImage', 'WebkitMaskOrigin',
                    'WebkitMaskPositionX', 'WebkitMaskPositionY', 'WebkitMaskRepeatX', 'WebkitMaskRepeatY', 'WebkitMaskSize']):
                fill_type = property_name if property_name == 'MaskSourceType' else property_name[len('Background'):]
                property_['custom_apply'] = 'fill_layer'
                property_['should_implement_apply_functions_in_cpp'] = True
                property_['custom_apply_args'] = {
                    'layer_type': 'Background' if 'Background' in property_name else 'Mask',
                    'fill_type': fill_type,
                    'fill_type_getter': 'Get' + fill_type if fill_type == "Image" else fill_type
                }
        property_['should_implement_apply_functions'] = (
            property_['is_property'] and
            not property_['longhands'] and
            not property_['direction_aware_options'] and
            not property_['builder_skip'] and
            (not (property_['custom_apply_functions_initial'] and
                  property_['custom_apply_functions_inherit'] and
                  property_['custom_apply_functions_value']) or
             'custom_apply' in property_))

    def h_includes(self, property_):
        if property_['alias_for']:
            yield "core/css/properties/css_unresolved_property.h"
        else:
            yield "core/css/properties/" + property_['namespace_group'] + ".h"
            if property_['direction_aware_options']:
                yield "core/style_property_shorthand.h"
            if property_['runtime_flag']:
                yield "platform/runtime_enabled_features.h"
            if property_['should_implement_apply_functions']:
                for include in self.apply_includes(property_):
                    yield include

    def cpp_includes(self, property_):
        if 'should_implement_apply_functions_in_cpp' in property_:
            for include in self.apply_includes(property_):
                yield include

    def apply_includes(self, property_):
        yield "core/css/resolver/StyleResolverState.h"
        yield "core/css/CSSPrimitiveValueMappings.h"
        if property_['converter'] == "CSSPrimitiveValue":
            yield "core/css/CSSPrimitiveValue.h"
            yield "core/css/CSSPrimitiveValueMappings.h"
        elif property_['converter'] == "CSSIdentifierValue":
            yield "core/css/CSSIdentifierValue.h"
        else:
            yield "core/css/CSSPrimitiveValueMappings.h"
            yield "core/css/resolver/StyleBuilderConverter.h"
        if property_['font']:
            yield "core/css/resolver/FontBuilder.h"
        elif property_['svg']:
            yield "core/css/CSSPrimitiveValueMappings.h"
            yield "core/style/ComputedStyle.h"
            yield "core/style/SVGComputedStyle.h"
        else:
            yield "core/style/ComputedStyle.h"
        if ('custom_apply_args' in property_ and
                property_['custom_apply_args'].get('modifier_type')
                in ['Width', 'Slice', 'Outset']):
            yield "core/css/properties/StyleBuildingUtils.h"
        if property_.get('custom_apply') == "fill_layer":
            yield "core/css/CSSValueList.h"


if __name__ == '__main__':
    json5_generator.Maker(CSSPropertiesWriter).main()
