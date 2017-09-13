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
import make_runtime_features
import name_utilities
import template_expander


# We want exactly the same parsing as RuntimeFeatureWriter
# but generate different files.
class InternalRuntimeFlagsWriter(make_runtime_features.RuntimeFeatureWriter):
    class_name = 'InternalRuntimeFlags'

    def __init__(self, json5_file_path):
        super(InternalRuntimeFlagsWriter, self).__init__(json5_file_path)
        basename = self.class_name
        if json5_generator.Writer.snake_case_source_files:
            basename = 'internal_runtime_flags'
        self._outputs = {(basename + '.idl'): self.generate_idl,
                         (basename + '.h'): self.generate_header,
                        }

    @template_expander.use_jinja('templates/' + class_name + '.idl.tmpl')
    def generate_idl(self):
        return {
            'features': self._features,
            'input_files': self._input_files,
            'standard_features': self._standard_features,
        }

    @template_expander.use_jinja('templates/' + class_name + '.h.tmpl')
    def generate_header(self):
        return {
            'features': self._features,
            'feature_sets': self._feature_sets(),
            'input_files': self._input_files,
            'standard_features': self._standard_features,
        }


if __name__ == '__main__':
    json5_generator.Maker(InternalRuntimeFlagsWriter).main()
