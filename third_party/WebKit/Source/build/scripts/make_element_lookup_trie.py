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

import sys
import StringIO
from collections import defaultdict

import in_generator
import template_expander


def _char_dict(tags, index):
    new_dict = defaultdict(list)
    for tag in tags:
        if index >= len(tag):
            continue
        new_dict[tag[index].lower()].append(tag)
    return new_dict


def _generate_if(name, index):
    conditions = []
    for i in range(index, len(name)):
        conditions.append("data[%d] == '%c'" % (i, name[i].lower()))
    return 'if (%s)\n' % ' && '.join(conditions)


def _print_switch(buf, tag_list, index):
    # FIXME: move this into a Jinja macro
    indent = '    ' * (index + 1)
    data = _char_dict(tag_list, index)
    # FIXME: No need to switch if there's only a single item in the data dict
    buf.write(indent + 'switch (data[%d]) {\n' % index)
    for char, tags in data.iteritems():
        buf.write(indent + "case '%c':\n" % char)
        if len(tags) > 1:
            _print_switch(buf, tags, index + 1)
        else:
            tag = tags[0]
            retval = 'return %sTag.localName().impl();\n' % tag
            if len(tag) == index + 1:
                buf.write(indent + '    ' + retval)
            else:
                buf.write(indent + '    ' + _generate_if(tag, index + 1))
                buf.write(indent + '        ' + retval)
                buf.write(indent + '    ' + 'return 0;\n')

    buf.write(indent + '}\n')
    buf.write(indent + 'return 0;\n')


class ElementLookupTrieWriter(in_generator.Writer):
    # FIXME: Inherit all these from somewhere.
    defaults = {
        'JSInterfaceName': None,
        'constructorNeedsCreatedByParser': None,
        'constructorNeedsFormElement': None,
        'contextConditional': None,
        'interfaceName': None,
        'noConstructor': None,
        'runtimeEnabled': None,
    }
    default_parameters = {
        'attrsNullNamespace': None,
        'namespace': '',
        'namespacePrefix': '',
        'namespaceURI': '',
        'fallbackInterfaceName': '',
        'fallbackJSInterfaceName': '',
    }

    def __init__(self, in_file_paths):
        super(ElementLookupTrieWriter, self).__init__(in_file_paths)
        self._tags = [entry['name'] for entry in self.in_file.name_dictionaries]
        self._namespace = self.in_file.parameters['namespace'].strip('"')
        self._outputs = {
            (self._namespace + "ElementLookupTrie.h"): self.generate_header,
            (self._namespace + "ElementLookupTrie.cpp"): self.generate_implementation,
        }

    @template_expander.use_jinja('ElementLookupTrie.h.tmpl')
    def generate_header(self):
        return {
            'namespace': self._namespace,
        }

    @template_expander.use_jinja('ElementLookupTrie.cpp.tmpl')
    def generate_implementation(self):
        size_dict = defaultdict(list)
        for tag in self._tags:
            size_dict[len(tag)].append(tag)

        buf = StringIO.StringIO()
        for length, tags in size_dict.iteritems():
            buf.write('case %d:\n' % length)
            _print_switch(buf, tags, 0)
        return {
            'namespace': self._namespace,
            'body': buf.getvalue(),
        }


if __name__ == "__main__":
    in_generator.Maker(ElementLookupTrieWriter).main(sys.argv)
