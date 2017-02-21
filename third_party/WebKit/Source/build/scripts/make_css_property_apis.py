#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import json5_generator
import template_expander
import make_style_builder

from collections import namedtuple, defaultdict
from json5_generator import Json5File
from make_style_builder import StyleBuilderWriter


class ApiMethod(namedtuple('ApiMethod', 'return_type,parameters,description')):
    pass


class ApiClass(namedtuple('ApiClass', 'index,classname,property_ids,methods_for_class')):
    pass


# Gets the classname for a given property.
def get_classname(property):
    if property['api_class'] is True:
        # This property had the generated_api_class flag set in CSSProperties.json5.
        return 'CSSPropertyAPI' + property['upper_camel_name']
    # This property has a specified class name.
    assert isinstance(property['api_class'], str), \
        ("api_class value for " + property['api_class'] + " should be None, True or a string")
    return property['api_class']


class CSSPropertyAPIWriter(StyleBuilderWriter):
    def __init__(self, json5_file_paths):
        super(CSSPropertyAPIWriter, self).__init__(json5_file_paths[0])
        # TODO(aazzam): Move the logic for loading CSSPropertyAPIMethods.json5 into a new class APIMethodsWriter().
        assert len(json5_file_paths) == 2,\
            'CSSPropertyAPIWriter requires 2 input json5 files files, got {}.'.format(len(json5_file_paths))

        self.css_property_api_methods = Json5File.load_from_files([json5_file_paths[1]], {}, {})

        self._outputs = {
            'CSSPropertyDescriptor.cpp': self.generate_property_descriptor_cpp,
            'CSSPropertyDescriptor.h': self.generate_property_descriptor_h,
            'CSSPropertyAPI.h': self.generate_property_api,
        }

        # Stores a map of API method name -> (return_type, parameters)
        self.all_api_methods = {}
        # Stores an ordered list of all API method names. This must match the
        # order they appear in the Descriptor object.
        self.ordered_api_method_names = []
        for api_method in self.css_property_api_methods.name_dictionaries:
            self.ordered_api_method_names.append(api_method['name'])
            # TODO(shend): wrap description to 72 chars
            self.all_api_methods[api_method['name']] = ApiMethod(
                return_type=api_method['return_type'],
                parameters=api_method['parameters'],
                description=api_method['description'],
            )

        # Temporary map of API classname to list of propertyIDs that the API class is for.
        properties_for_class = defaultdict(list)
        # Map of API classname to list of methods implemented by that class.
        self.methods_for_classes = defaultdict(list)
        for property_ in self.properties().values():
            if property_['api_class'] is None:
                continue
            classname = get_classname(property_)
            properties_for_class[classname].append(property_['property_id'])
            # For api_classes that contain multiple properties, combine all implemented properties.
            # This list contains duplicate entries, but is only used to check if a method is
            # implemented for an api_class.
            self.methods_for_classes[classname] += property_['api_methods']
            self._outputs[classname + '.h'] = self.generate_property_api_h_builder(classname)

        # Stores a list of classes with elements (index, classname, [propertyIDs, ..], [api_methods, ...]).
        self._api_classes = []
        for i, classname in enumerate(properties_for_class.keys()):
            self._api_classes.append(ApiClass(
                index=i + 1,
                classname=classname,
                property_ids=properties_for_class[classname],
                methods_for_class=self.methods_for_classes[classname]
            ))

    @template_expander.use_jinja('CSSPropertyDescriptor.cpp.tmpl')
    def generate_property_descriptor_cpp(self):
        return {
            'api_classes': self._api_classes,
            'ordered_api_method_names': self.ordered_api_method_names,
        }

    @template_expander.use_jinja('CSSPropertyDescriptor.h.tmpl')
    def generate_property_descriptor_h(self):
        return {
            'ordered_api_method_names': self.ordered_api_method_names,
            'all_api_methods': self.all_api_methods,
        }

    @template_expander.use_jinja('CSSPropertyAPI.h.tmpl')
    def generate_property_api(self):
        return {
            'ordered_api_method_names': self.ordered_api_method_names,
            'all_api_methods': self.all_api_methods,
        }

    # Provides a function object given the classname of the property.
    def generate_property_api_h_builder(self, api_classname):
        @template_expander.use_jinja('CSSPropertyAPIFiles.h.tmpl')
        def generate_property_api_h():
            return {
                'api_classname': api_classname,
                'methods_for_class': self.methods_for_classes[api_classname],
                'all_api_methods': self.all_api_methods,
            }
        return generate_property_api_h

if __name__ == '__main__':
    json5_generator.Maker(CSSPropertyAPIWriter).main()
