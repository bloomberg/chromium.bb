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
import re

import in_generator
from in_generator import Maker
import license


HEADER_TEMPLATE = """%(license)s

#ifndef %(class_name)sHeaders_h
#define %(class_name)sHeaders_h

%(includes)s

#endif // %(class_name)sHeaders_h
"""


INTERFACES_HEADER_TEMPLATE = """%(license)s

#ifndef %(class_name)sInterfaces_h
#define %(class_name)sInterfaces_h

%(declare_conditional_macros)s

#define %(macro_style_name)s_INTERFACES_FOR_EACH(macro) \\
    \\
%(unconditional_macros)s
    \\
%(conditional_macros)s

#endif // %(class_name)sInterfaces_h
"""


def _to_macro_style(name):
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()


class Writer(in_generator.Writer):
    def __init__(self, in_file_path, enabled_conditions):
        super(Writer, self).__init__(in_file_path, enabled_conditions)
        self.class_name = self.in_file.parameters['namespace'].strip('"')
        self._entries_by_conditional = {}
        self._unconditional_entries = []
        self._sort_entries_by_conditional()
        self._outputs = {(self.class_name + "Headers.h"): self.generate_headers_header,
                         (self.class_name + "Interfaces.h"): self.generate_interfaces_header,
                        }

    def _sort_entries_by_conditional(self):
        unconditional_names = set()
        for entry in self.in_file.name_dictionaries:
            conditional = entry['Conditional']
            if not conditional:
                name = self._class_name_for_entry(entry)
                if name in unconditional_names:
                    continue
                unconditional_names.add(name)
                self._unconditional_entries.append(entry)
                continue
        for entry in self.in_file.name_dictionaries:
            name = self._class_name_for_entry(entry)
            if name in unconditional_names:
                continue
            conditional = entry['Conditional']
            if not conditional in self._entries_by_conditional:
                self._entries_by_conditional[conditional] = []
            self._entries_by_conditional[conditional].append(entry)

    def _class_name_for_entry(self, entry):
        if entry['ImplementedAs']:
            return entry['ImplementedAs']
        return os.path.basename(entry['name'])

    def _headers_header_include_path(self, entry):
        if entry['ImplementedAs']:
            path = os.path.dirname(entry['name'])
            if len(path):
                path += '/'
            path += entry['ImplementedAs']
        else:
            path = entry['name']
        return path + '.h'

    def _headers_header_includes(self, entries):
        includes = dict()
        for entry in entries:
            class_name = self._class_name_for_entry(entry)
            # Avoid duplicate includes.
            if class_name in includes:
                continue
            include = '#include "%(path)s"\n#include "V8%(js_name)s.h"' % {
                'path': self._headers_header_include_path(entry),
                'js_name': os.path.basename(entry['name']),
            }
            includes[class_name] = self.wrap_with_condition(include, entry['Conditional'])
        return includes.values()

    def generate_headers_header(self):
        return HEADER_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'includes': '\n'.join(self._headers_header_includes(self.in_file.name_dictionaries)),
        }

    def _declare_one_conditional_macro(self, conditional, entries):
        macro_name = '%(macro_style_name)s_INTERFACES_FOR_EACH_%(conditional)s' % {
            'macro_style_name': _to_macro_style(self.class_name),
            'conditional': conditional,
        }
        return self.wrap_with_condition("""#define %(macro_name)s(macro) \\
%(declarations)s

#else
#define %(macro_name)s(macro)""" % {
            'macro_name': macro_name,
            'declarations': '\n'.join(sorted(set([
                '    macro(%(name)s) \\' % {'name': self._class_name_for_entry(entry)}
                for entry in entries]))),
        }, conditional)

    def _declare_conditional_macros(self):
        return '\n'.join([
            self._declare_one_conditional_macro(conditional, entries)
            for conditional, entries in self._entries_by_conditional.items()])

    def _unconditional_macro(self, entry):
        return '    macro(%(name)s) \\' % {'name': self._class_name_for_entry(entry)}

    def _conditional_macros(self, conditional):
        return '    %(macro_style_name)s_INTERFACES_FOR_EACH_%(conditional)s(macro) \\' % {
            'macro_style_name': _to_macro_style(self.class_name),
            'conditional': conditional,
        }

    def generate_interfaces_header(self):
        return INTERFACES_HEADER_TEMPLATE % {
            'license': license.license_for_generated_cpp(),
            'class_name': self.class_name,
            'macro_style_name': _to_macro_style(self.class_name),
            'declare_conditional_macros': self._declare_conditional_macros(),
            'unconditional_macros': '\n'.join(sorted(set(map(self._unconditional_macro, self._unconditional_entries)))),
            'conditional_macros': '\n'.join(map(self._conditional_macros, self._entries_by_conditional.keys())),
        }
