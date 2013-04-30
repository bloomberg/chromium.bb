#!/usr/bin/env python

import os.path
import sys
import shutil

from in_file import InFile

# FIXME: We should either not use license blocks in generated files
# or we should read this from some central license file.
LICENSE = """/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"""

HEADER_TEMPLATE = """%(license)s
#ifndef %(class_name)s_h
#define %(class_name)s_h

namespace WebCore {

// A class that stores static enablers for all experimental features.

class %(class_name)s {
public:
%(method_declarations)s
private:
    RuntimeEnabledFeatures() { }

%(storage_declarations)s
};

} // namespace WebCore

#endif // %(class_name)s_h
"""

IMPLEMENTATION_TEMPLATE = """%(license)s
#include "config.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

%(storage_definitions)s

} // namespace WebCore
"""

class RuntimeFeatureWriter(object):
    def __init__(self, in_file_path):
        # Assume that the class should be called the same as the file.
        self.class_name, _ = os.path.splitext(os.path.basename(in_file_path))
        defaults = {
            'condition' : None,
            'depends_on' : [],
            'default': 'false',
            'custom': False,
        }
        self._all_features = InFile.load_from_path(in_file_path, defaults).name_dictionaries
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

    def _wrap_with_condition(self, string, condition):
        if not condition:
            return string
        return "#if ENABLE(%(condition)s)\n%(string)s\n#endif" % { 'condition' : condition, 'string' : string }

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
        return self._wrap_with_condition(declaration, feature['condition'])

    def _generate_header(self):
        return HEADER_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : LICENSE,
            'method_declarations' : "\n".join(map(self._method_declaration, self._all_features)),
            'storage_declarations' : "\n".join(map(self._storage_declarations, self._non_custom_features)),
        }

    def _storage_definition(self, feature):
        definition = "bool RuntimeEnabledFeatures::is%(name)sEnabled = %(default)s;" % feature
        return self._wrap_with_condition(definition, feature['condition'])

    def _generate_implementation(self):
        return IMPLEMENTATION_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : LICENSE,
            'storage_definitions' : "\n".join(map(self._storage_definition, self._non_custom_features)),
        }

    def _forcibly_create_text_file_at_path_with_contents(self, file_path, contents):
        # FIXME: This method can be made less force-full anytime after 6/1/2013.
        # A gyp error was briefly checked into the tree, causing
        # a directory to have been generated in place of one of
        # our output files.  Clean up after that error so that
        # all users don't need to clobber their output directories.
        shutil.rmtree(file_path, ignore_errors=True)
        # The build system should ensure our output directory exists, but just in case.
        directory = os.path.dirname(file_path)
        if not os.path.exists(directory):
            os.makedirs(directory)

        with open(file_path, "w") as file_to_write:
            file_to_write.write(contents)

    def write_header(self, output_dir):
        header_path = os.path.join(output_dir, self.class_name + ".h")
        self._forcibly_create_text_file_at_path_with_contents(header_path, self._generate_header())

    def write_implmentation(self, output_dir):
        implmentation_path = os.path.join(output_dir, self.class_name + ".cpp")
        self._forcibly_create_text_file_at_path_with_contents(implmentation_path, self._generate_implementation())


class MakeRuntimeFeatures(object):
    def main(self, argv):
        script_name = os.path.basename(argv[0])
        args = argv[1:]
        if len(args) < 1:
            print "USAGE: %i INPUT_FILE [OUTPUT_DIRECTORY]" % script_name
            exit(1)
        output_dir = args[1] if len(args) > 1 else os.getcwd()

        writer = RuntimeFeatureWriter(args[0])
        writer.write_header(output_dir)
        writer.write_implmentation(output_dir)


if __name__ == "__main__":
    MakeRuntimeFeatures().main(sys.argv)
