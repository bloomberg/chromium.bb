#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import media_feature_symbol
import json5_generator
import template_expander
import name_utilities
import sys


class MakeMediaFeaturesWriter(json5_generator.Writer):
    default_metadata = {
        'namespace': '',
        'export': '',
    }
    filters = {
        'symbol': media_feature_symbol.getMediaFeatureSymbolWithSuffix(''),
        'to_macro_style': name_utilities.to_macro_style,
    }

    def __init__(self, json5_file_path):
        super(MakeMediaFeaturesWriter, self).__init__(json5_file_path)

        self._outputs = {
            ('MediaFeatures.h'): self.generate_header,
        }
        self._template_context = {
            'entries': self.json5_file.name_dictionaries,
        }

    @template_expander.use_jinja('MediaFeatures.h.tmpl', filters=filters)
    def generate_header(self):
        return self._template_context

if __name__ == '__main__':
    json5_generator.Maker(MakeMediaFeaturesWriter).main()
