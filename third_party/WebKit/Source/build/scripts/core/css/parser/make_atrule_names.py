#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '../../..'))

import gperf
import json5_generator
import template_expander

from name_utilities import upper_camel_case


class AtRuleNamesWriter(json5_generator.Writer):
    """
    Generates AtRuleNames. This class provides utility methods for parsing
    @rules (e.g. @font-face, @viewport, etc)
    """
    def __init__(self, json5_file_paths):
        super(AtRuleNamesWriter, self).__init__(json5_file_paths)

        self._outputs = {
            'AtRuleDescriptors.h': self.generate_header,
            'AtRuleDescriptors.cpp': self.generate_implementation
        }

        self._descriptors = self.json5_file.name_dictionaries
        self._character_offsets = []

        # AtRuleDescriptorID::Invalid is 0.
        first_descriptor_id = 1
        # Aliases are resolved immediately at parse time, and thus don't appear
        # in the enum.
        self._descriptors_count = len(self._descriptors) + first_descriptor_id
        chars_used = 0
        self._longest_name_length = 0
        for offset, descriptor in enumerate(self._descriptors):
            descriptor['upper_camel_name'] = upper_camel_case(
                descriptor['name'])
            descriptor['enum_value'] = first_descriptor_id + offset
            self._character_offsets.append(chars_used)
            chars_used += len(descriptor['name'])
            self._longest_name_length = max(
                len(descriptor['name']),
                len(descriptor['alias']),
                self._longest_name_length)

    @template_expander.use_jinja(
        'core/css/parser/templates/AtRuleDescriptors.h.tmpl')
    def generate_header(self):
        return {
            'descriptors': self._descriptors,
            'descriptors_count': self._descriptors_count
        }

    @gperf.use_jinja_gperf_template(
        'core/css/parser/templates/AtRuleDescriptors.cpp.tmpl')
    def generate_implementation(self):
        return {
            'descriptors': self._descriptors,
            'descriptor_offsets': self._character_offsets,
            'descriptors_count': len(self._descriptors),
            'longest_name_length': self._longest_name_length,
            'gperf_path': self.gperf_path
        }

if __name__ == '__main__':
    json5_generator.Maker(AtRuleNamesWriter).main()
