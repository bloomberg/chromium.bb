#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import in_generator
import template_expander
import make_style_builder

from collections import namedtuple, defaultdict


# Gets the classname for a given property.
def get_classname(property):
    if property['api_class'] is True:
        # This property had the generated_api_class flag set in CSSProperties.in.
        return 'CSSPropertyAPI' + property['upper_camel_name']
    # This property has a specified class name.
    assert isinstance(property['api_class'], str), \
        ("api_class value for " + property['api_class'] + " should be None, True or a string")
    return property['api_class']


class CSSPropertyAPIWriter(make_style_builder.StyleBuilderWriter):
    def __init__(self, in_file_path):
        super(CSSPropertyAPIWriter, self).__init__(in_file_path)
        self._outputs = {
            'CSSPropertyDescriptor.cpp': self.generate_property_descriptor_cpp,
        }

        # Temporary map of API classname to list of propertyIDs that the API class is for.
        properties_for_class = defaultdict(list)
        for property in self._properties.values():
            if property['api_class'] is None:
                continue
            classname = get_classname(property)
            properties_for_class[classname].append(property['property_id'])

        # Stores a list of classes with elements (index, classname, [propertyIDs, ..]).
        self._api_classes = []

        ApiClass = namedtuple('ApiClass', ('index', 'classname', 'property_ids'))
        for i, classname in enumerate(properties_for_class.keys()):
            self._api_classes.append(ApiClass(
                index=i + 1,
                classname=classname,
                property_ids=properties_for_class[classname]
            ))

    @template_expander.use_jinja('CSSPropertyDescriptor.cpp.tmpl')
    def generate_property_descriptor_cpp(self):
        return {
            'api_classes': self._api_classes,
        }

if __name__ == '__main__':
    in_generator.Maker(CSSPropertyAPIWriter).main(sys.argv)
