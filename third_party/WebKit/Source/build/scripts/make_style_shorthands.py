#!/usr/bin/env python
# Copyright (C) 2013 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import re
import sys
from collections import defaultdict

import in_generator
import template_expander


class StylePropertyShorthandWriter(in_generator.Writer):
    class_name = 'StylePropertyShorthand'

    defaults = {
        'longhands': "",
        'runtimeEnabledShorthand': None,
    }

    def __init__(self, in_files):
        super(StylePropertyShorthandWriter, self).__init__(in_files)
        self._outputs = {("StylePropertyShorthand.cpp"): self.generate_style_property_shorthand_cpp, ("StylePropertyShorthand.h"): self.generate_style_property_shorthand_h}

        self._properties = self.in_file.name_dictionaries
        self._longhand_dictionary = defaultdict(list)

        for property in self._properties:
            cc = self._camelcase_property_name(property["name"])
            property["property_id"] = self._create_css_property_name_enum_value(cc)
            cc = cc[0].lower() + cc[1:]
            property["camel_case_name"] = cc
            longhands = property["longhands"].split(';')
            property["camel_case_longhands"] = list()
            for longhand in longhands:
                longhand = self._camelcase_property_name(longhand)
                longhand = self._create_css_property_name_enum_value(longhand)
                property["camel_case_longhands"].append(longhand)
                self._longhand_dictionary[longhand].append(property)
            if property["runtimeEnabledShorthand"] is not None:
                lowerFirstConditional = self._lower_first(property["runtimeEnabledShorthand"])
                property["runtime_conditional_getter"] = "%sEnabled" % lowerFirstConditional
        self._properties = dict((property["property_id"], property) for property in self._properties)

# FIXME: some of these might be better in a utils file
    def _camelcase_property_name(self, property_name):
        return re.sub(r'(^[^-])|-(.)', lambda match: (match.group(1) or match.group(2)).upper(), property_name)

    def _create_css_property_name_enum_value(self, property_name):
        return "CSSProperty" + property_name

    def _lower_first(self, string):
        lowered = string[0].lower() + string[1:]
        lowered = lowered.replace("cSS", "css")
        return lowered

    @template_expander.use_jinja("StylePropertyShorthand.cpp.tmpl")
    def generate_style_property_shorthand_cpp(self):
        return {
            "properties": self._properties,
            "longhands_dictionary": self._longhand_dictionary,
        }

    @template_expander.use_jinja("StylePropertyShorthand.h.tmpl")
    def generate_style_property_shorthand_h(self):
        return {
            "properties": self._properties,
        }

if __name__ == "__main__":
    in_generator.Maker(StylePropertyShorthandWriter).main(sys.argv)
