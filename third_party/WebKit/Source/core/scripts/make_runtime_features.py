#!/usr/bin/env python
# Copyright (C) 2013 Google Inc. All rights reserved.
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

import os.path
import sys

from in_file import InFile
import in_generator
import license


HEADER_TEMPLATE = """%(license)s
#ifndef %(class_name)s_h
#define %(class_name)s_h

namespace WebCore {

// A class that stores static enablers for all experimental features.

class %(class_name)s {
public:
%(method_declarations)s
private:
    %(class_name)s() { }

%(storage_declarations)s
};

} // namespace WebCore

#endif // %(class_name)s_h
"""

IMPLEMENTATION_TEMPLATE = """%(license)s
#include "config.h"
#include "%(class_name)s.h"

namespace WebCore {

%(storage_definitions)s

} // namespace WebCore
"""

class RuntimeFeatureWriter(in_generator.Writer):
    class_name = "RuntimeEnabledFeatures"
    defaults = {
        'condition' : None,
        'depends_on' : [],
        'custom': False,
    }

    def __init__(self, in_file_path):
        super(RuntimeFeatureWriter, self).__init__(in_file_path)
        self._all_features = self.in_file.name_dictionaries
        # Make sure the resulting dictionaries have all the keys we expect.
        for feature in self._all_features:
            feature['first_lowered_name'] = self._lower_first(feature['name'])
            # Most features just check their isFooEnabled bool
            # but some depend on more than one bool.
            enabled_condition = "is%sEnabled" % feature['name']
            for dependant_name in feature['depends_on']:
                enabled_condition += " && is%sEnabled" % dependant_name
            feature['enabled_condition'] = enabled_condition
        self._non_custom_features = filter(lambda feature: not feature['custom'], self._all_features)

    def _lower_first(self, string):
        lowered = string[0].lower() + string[1:]
        lowered = lowered.replace("cSS", "css")
        lowered = lowered.replace("iME", "ime")
        return lowered

    def _method_declaration(self, feature):
        if feature['custom']:
            return "    static bool %(first_lowered_name)sEnabled();\n" % feature
        unconditional = """    static void set%(name)sEnabled(bool isEnabled) { is%(name)sEnabled = isEnabled; }
    static bool %(first_lowered_name)sEnabled() { return %(enabled_condition)s; }
"""
        conditional = "#if ENABLE(%(condition)s)\n" + unconditional + """#else
    static void set%(name)sEnabled(bool) { }
    static bool %(first_lowered_name)sEnabled() { return false; }
#endif
"""
        template = conditional if feature['condition'] else unconditional
        return template % feature

    def _storage_declarations(self, feature):
        declaration = "    static bool is%(name)sEnabled;" % feature
        return self.wrap_with_condition(declaration, feature['condition'])

    def generate_header(self):
        return HEADER_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : license.license_for_generated_cpp(),
            'method_declarations' : "\n".join(map(self._method_declaration, self._all_features)),
            'storage_declarations' : "\n".join(map(self._storage_declarations, self._non_custom_features)),
        }

    def _storage_definition(self, feature):
        definition = "bool RuntimeEnabledFeatures::is%(name)sEnabled = false;" % feature
        return self.wrap_with_condition(definition, feature['condition'])

    def generate_implementation(self):
        return IMPLEMENTATION_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : license.license_for_generated_cpp(),
            'storage_definitions' : "\n".join(map(self._storage_definition, self._non_custom_features)),
        }


if __name__ == "__main__":
    in_generator.Maker(RuntimeFeatureWriter).main(sys.argv)
