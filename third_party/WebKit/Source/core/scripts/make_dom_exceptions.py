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
import license


HEADER_TEMPLATE = """%(license)s

#ifndef %(class_name)s_h
#define %(class_name)s_h

namespace WebCore {

typedef int ExceptionCode;

enum ExceptionType {
%(exception_types)s
};

struct ExceptionCodeDescription {
    explicit ExceptionCodeDescription(ExceptionCode);

    // |name| is the exception name, also intended for use in exception
    // description strings; 0 if name not known; maximum length is 27
    // characters.
    const char* name;

    // |description| is the exception description, intended for use in
    // exception strings. It is a more readable explanation of error.
    const char* description;

    // |code| is the numeric value of the exception within a particular type.
    int code;

    ExceptionType type;
};

} // namespace WebCore

#endif // %(class_name)s_h
"""


IMPLEMENTATION_TEMPLATE = """%(license)s

#include "config.h"
#include "%(class_name)s.h"

#include "ExceptionCode.h"

%(includes)s

#include "modules/indexeddb/IDBDatabaseException.h"

namespace WebCore {

ExceptionCodeDescription::ExceptionCodeDescription(ExceptionCode ec)
{
    ASSERT(ec);

%(description_initalizations)s

    // FIXME: This special case for IDB is undesirable. It is the first usage
    // of "new style" DOMExceptions where there is no IDL type, but there are
    // API-specific exception names and/or messages. Consider refactoring back
    // into the code generator when a common pattern emerges.
    if (IDBDatabaseException::initializeDescription(ec, this))
        return;

    if (DOMCoreException::initializeDescription(ec, this))
        return;

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
"""


class ExceptionCodeDescriptionWriter(name_macros.Writer):
    defaults = {
        'implementedAs': None,
        'conditional': None,
    }
    default_parameters = {
        'namespace': '',
    }

    def __init__(self, in_file_path, enabled_conditions):
        super(ExceptionCodeDescriptionWriter, self).__init__(in_file_path, enabled_conditions)
        self._outputs[(self.class_name + ".cpp")] = self.generate_implementation
        self._outputs[(self.class_name + ".h")] = self.generate_header

    def _exceptions(self):
        return self.in_file.name_dictionaries

    def _exception_type(self, exception):
        return self.wrap_with_condition('    ' + self._class_name_for_entry(exception) + 'Type,', exception['conditional'])

    def generate_header(self):
        return HEADER_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'exception_types': '\n'.join(map(self._exception_type, self._exceptions())),
        }

    def _include(self, exception):
        include = '#include "' + self._headers_header_include_path(exception) + '"'
        return self.wrap_with_condition(include, exception['conditional'])

    def _description_initalization(self, exception):
        name = os.path.basename(exception['name'])
        if name == 'DOMException':
            return ''  # DOMException needs to be last because it's a catch-all.
        description_initalization = """    if (%(name)s::initializeDescription(ec, this))
        return;""" % {'name': name}
        return self.wrap_with_condition(description_initalization, exception['conditional'])

    def generate_implementation(self):
        return IMPLEMENTATION_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'includes': '\n'.join(map(self._include, self._exceptions())),
            'description_initalizations': '\n'.join(map(self._description_initalization, self._exceptions())),
        }


if __name__ == "__main__":
    name_macros.Maker(ExceptionCodeDescriptionWriter).main(sys.argv)
