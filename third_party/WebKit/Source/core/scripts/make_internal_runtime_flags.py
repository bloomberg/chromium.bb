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

import in_generator
import license
import make_runtime_features


IDL_TEMPLATE = """
%(license)s
[
    ImplementationLacksVTable
] interface %(class_name)s {
%(attribute_declarations)s
};
"""

HEADER_TEMPLATE = """%(license)s
#ifndef %(class_name)s_h
#define %(class_name)s_h

#include "RuntimeEnabledFeatures.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class %(class_name)s : public RefCounted<%(class_name)s> {
public:
    static PassRefPtr<%(class_name)s> create()
    {
        return adoptRef(new %(class_name)s);
    }

%(method_declarations)s

private:
    %(class_name)s() { }
};

} // namespace WebCore

#endif // %(class_name)s_h
"""

# We want exactly the same parsing as RuntimeFeatureWriter
# but generate different files.
class InternalRuntimeFlagsWriter(make_runtime_features.RuntimeFeatureWriter):
    class_name = "InternalRuntimeFlags"

    def __init__(self, *args, **kwargs):
        make_runtime_features.RuntimeFeatureWriter.__init__(self, *args, **kwargs)
        for feature in self._all_features:
            feature['idl_attributes'] = "[Conditional=%(condition)s]" % feature if feature['condition'] else ""

    def _attribute_declaration(self, feature):
        # Currently assuming that runtime flags cannot be changed after startup
        # it's possible that some can be and should be conditionally readonly.
        return "    %(idl_attributes)s readonly attribute boolean %(first_lowered_name)sEnabled;" % feature

    def generate_idl(self):
        return IDL_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : license.license_for_generated_cpp(),
            'attribute_declarations': "\n".join(map(self._attribute_declaration, self._non_custom_features))
        }

    def _method_declaration(self, feature):
        # Setting after startup does not work for most runtime flags, but we
        # could add an option to print setters for ones which do:
        # void set%(name)sEnabled(bool isEnabled) { RuntimeEnabledFeatures::set%(name)sEnabled(isEnabled); }
        # If we do that, we also need to respect Internals::resetToConsistentState.
        declaration = """    bool %(first_lowered_name)sEnabled() { return RuntimeEnabledFeatures::%(first_lowered_name)sEnabled(); }
""" % feature
        return self.wrap_with_condition(declaration, feature['condition'])

    def generate_header(self):
        return HEADER_TEMPLATE % {
            'class_name' : self.class_name,
            'license' : license.license_for_generated_cpp(),
            'method_declarations' : "\n".join(map(self._method_declaration, self._non_custom_features)),
        }

    def generate_implementation(self):
        return None


if __name__ == "__main__":
    in_generator.Maker(InternalRuntimeFlagsWriter).main(sys.argv)
