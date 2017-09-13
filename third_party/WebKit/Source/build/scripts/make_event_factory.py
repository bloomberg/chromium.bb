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

import json5_generator
import license
import name_utilities
import template_expander


HEADER_TEMPLATE = """%(license)s

#ifndef Event%(suffix)sHeaders_h
#define Event%(suffix)sHeaders_h
%(base_header_for_suffix)s
%(includes)s

#endif // Event%(suffix)sHeaders_h
"""


# All events on the following whitelist are matched case-insensitively
# in createEvent.
#
# https://dom.spec.whatwg.org/#dom-document-createevent
def create_event_whitelist(name):
    return (name == 'HTMLEvents'
            or name == 'Event'
            or name == 'Events'
            or name.startswith('UIEvent')
            or name.startswith('CustomEvent')
            or name == 'KeyboardEvent'
            or name == 'MessageEvent'
            or name.startswith('MouseEvent')
            or name == 'TouchEvent')


# All events on the following whitelist are matched case-insensitively
# in createEvent and are measured using UseCounter.
#
# TODO(foolip): All events on this list should either be added to the spec and
# moved to the above whitelist (causing them to be matched case-insensitively)
# or be deprecated/removed. https://crbug.com/569690
def create_event_measure_whitelist(name):
    return (name == 'AnimationEvent'
            or name == 'BeforeUnloadEvent'
            or name == 'CloseEvent'
            or name == 'CompositionEvent'
            or name == 'DeviceMotionEvent'
            or name == 'DeviceOrientationEvent'
            or name == 'DragEvent'
            or name == 'ErrorEvent'
            or name == 'FocusEvent'
            or name == 'HashChangeEvent'
            or name == 'IDBVersionChangeEvent'
            or name == 'KeyboardEvents'
            or name == 'MutationEvent'
            or name == 'MutationEvents'
            or name == 'PageTransitionEvent'
            or name == 'PopStateEvent'
            or name == 'StorageEvent'
            or name == 'SVGEvents'
            or name == 'TextEvent'
            or name == 'TrackEvent'
            or name == 'TransitionEvent'
            or name == 'WebGLContextEvent'
            or name == 'WheelEvent')


def measure_name(name):
    return 'DocumentCreateEvent' + name


class EventFactoryWriter(json5_generator.Writer):
    default_parameters = {
        'ImplementedAs': {},
        'RuntimeEnabled': {},
    }
    default_metadata = {
        'export': '',
        'namespace': '',
        'suffix': '',
    }
    filters = {
        'cpp_name': name_utilities.cpp_name,
        'lower_first': name_utilities.lower_first,
        'script_name': name_utilities.script_name,
        'create_event_whitelist': create_event_whitelist,
        'create_event_measure_whitelist': create_event_measure_whitelist,
        'measure_name': measure_name,
    }

    def __init__(self, json5_file_path):
        super(EventFactoryWriter, self).__init__(json5_file_path)
        self.namespace = self.json5_file.metadata['namespace'].strip('"')
        assert self.namespace == 'Event', 'namespace field should be "Event".'
        self.suffix = self.json5_file.metadata['suffix'].strip('"')
        self._outputs = {('Event%sHeaders.h' % self.suffix): self.generate_headers_header,
                         ('Event%s.cpp' % self.suffix): self.generate_implementation,
                        }

    def _fatal(self, message):
        print 'FATAL ERROR: ' + message
        exit(1)

    def _get_basename(self, name, prefix=None):
        if json5_generator.Writer.snake_case_source_files:
            if prefix:
                return '%s_%s' % (prefix.lower(), name_utilities.snake_case(name))
            return name_utilities.snake_case(name)
        if prefix:
            return '%s%s' % (prefix, name)
        return name

    def _headers_header_include_path(self, entry):
        path = os.path.dirname(entry['name'])
        if len(path):
            path += '/'
        if entry['ImplementedAs']:
            path += self._get_basename(entry['ImplementedAs'])
        else:
            path += self._get_basename(os.path.basename(entry['name']))
        return path + '.h'

    def _headers_header_includes(self, entries):
        includes = dict()
        for entry in entries:
            cpp_name = name_utilities.cpp_name(entry)
            # Avoid duplicate includes.
            if cpp_name in includes:
                continue
            if self.suffix == 'Modules':
                subdir_name = 'modules'
            else:
                subdir_name = 'core'
            binding_name = self._get_basename(name_utilities.script_name(entry), prefix='V8')
            includes[cpp_name] = '#include "%(path)s"\n#include "bindings/%(subdir_name)s/v8/%(binding_name)s.h"' % {
                'binding_name': binding_name,
                'path': self._headers_header_include_path(entry),
                'subdir_name': subdir_name,
            }
        return includes.values()

    def generate_headers_header(self):
        base_header_for_suffix = ''
        if self.suffix:
            base_header_for_suffix = '\n#include "core/EventHeaders.h"\n'
        return HEADER_TEMPLATE % {
            'input_files': self._input_files,
            'license': license.license_for_generated_cpp(),
            'suffix': self.suffix,
            'base_header_for_suffix': base_header_for_suffix,
            'includes': '\n'.join(self._headers_header_includes(self.json5_file.name_dictionaries)),
        }

    @template_expander.use_jinja('templates/EventFactory.cpp.tmpl', filters=filters)
    def generate_implementation(self):
        return {
            'input_files': self._input_files,
            'suffix': self.suffix,
            'events': self.json5_file.name_dictionaries,
        }


if __name__ == "__main__":
    json5_generator.Maker(EventFactoryWriter).main()
