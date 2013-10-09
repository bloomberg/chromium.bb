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
from name_utilities import lower_first
import license


IMPLEMENTATION_TEMPLATE = """%(license)s
#include "config.h"
#include "core/events/%(class_name)sFactory.h"

#include "%(class_name)sHeaders.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

PassRefPtr<%(class_name)s> %(class_name)sFactory::create(const String& type)
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
        'EnabledAtRuntime': None,
    }
    default_parameters = {
        'namespace': '',
    }

    def __init__(self, in_file_path, enabled_conditions):
        super(EventFactoryWriter, self).__init__(in_file_path, enabled_conditions)
        self._outputs[(self.class_name + ".cpp")] = self.generate_implementation

    def _events(self):
        return self.in_file.name_dictionaries

    def _factory_implementation(self, event):
        if event['EnabledAtRuntime']:
            runtime_condition = ' && RuntimeEnabledFeatures::%s()' % lower_first(event['EnabledAtRuntime'])
        else:
            runtime_condition = ''
        name = os.path.basename(event['name'])
        class_name = self._class_name_for_entry(event)
        implementation = """    if (type == "%(name)s"%(runtime_condition)s)
        return %(class_name)s::create();""" % {
            'name': name,
            'runtime_condition': runtime_condition,
            'class_name': class_name,
        }
        return self.wrap_with_condition(implementation, event['Conditional'])

    def generate_implementation(self):
        return IMPLEMENTATION_TEMPLATE % {
            'class_name': self.class_name,
            'license': license.license_for_generated_cpp(),
            'factory_implementation': "\n".join(map(self._factory_implementation, self._events())),
        }


if __name__ == "__main__":
    name_macros.Maker(EventFactoryWriter).main(sys.argv)
