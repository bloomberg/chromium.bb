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
import shutil

from in_file import InFile
import name_macros
import name_utilities
import license


IMPLEMENTATION_TEMPLATE = """%(license)s
#include "config.h"
#include "core/events/%(namespace)sFactory.h"

#include "%(namespace)sHeaders.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

PassRefPtr<%(namespace)s> %(namespace)sFactory::create(const String& type)
{
%(factory_implementation)s
    return 0;
}

} // namespace WebCore
"""


class EventFactoryWriter(name_macros.Writer):
    defaults = {
        'ImplementedAs': None,
        'Conditional': None,
        'RuntimeEnabled': None,
    }
    default_parameters = {
        'namespace': '',
    }

    def __init__(self, in_file_path, enabled_conditions):
        super(EventFactoryWriter, self).__init__(in_file_path, enabled_conditions)
        self._outputs[(self.namespace + ".cpp")] = self.generate_implementation

    def _events(self):
        return self.in_file.name_dictionaries

    def _factory_implementation(self, event):
        if event['RuntimeEnabled']:
            runtime_condition = ' && RuntimeEnabledFeatures::%s()' % name_utilities.lower_first(event['RuntimeEnabled'])
        else:
            runtime_condition = ''
        script_name = name_utilities.script_name(event)
        cpp_name = name_utilities.cpp_name(event)
        implementation = """    if (type == "%(script_name)s"%(runtime_condition)s)
        return %(cpp_name)s::create();""" % {
            'script_name': script_name,
            'runtime_condition': runtime_condition,
            'cpp_name': cpp_name,
        }
        return self.wrap_with_condition(implementation, event['Conditional'])

    def generate_implementation(self):
        return IMPLEMENTATION_TEMPLATE % {
            'namespace': self.namespace,
            'license': license.license_for_generated_cpp(),
            'factory_implementation': "\n".join(map(self._factory_implementation, self._events())),
        }


if __name__ == "__main__":
    name_macros.Maker(EventFactoryWriter).main(sys.argv)
