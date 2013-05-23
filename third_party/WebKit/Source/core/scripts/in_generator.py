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
import shutil

from in_file import InFile
import template_expander


class Writer(object):
    # Subclasses should override.
    class_name = None
    defaults = None
    valid_values = None
    default_parameters = None

    def __init__(self, in_file_path):
        self.in_file = InFile.load_from_path(in_file_path, self.defaults, self.valid_values, self.default_parameters)

    # Subclasses should override.
    def generate_header(self):
        return ''

    # Subclasses should override.
    def generate_interfaces_header(self):
        return ''

    # Subclasses should override.
    def generate_headers_header(self):
        return ''

    # Subclasses should override.
    def generate_implementation(self):
        return ''

    # Subclasses should override.
    def generate_idl(self):
        return ''

    def wrap_with_condition(self, string, condition):
        if not condition:
            return string
        return "#if ENABLE(%(condition)s)\n%(string)s\n#endif" % { 'condition' : condition, 'string' : string }

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

    def _write_file(self, output_dir, generator, file_name):
        contents = generator()
        if type(contents) is dict:
            contents = template_expander.apply_template(file_name + ".tmpl", contents)
        if not contents:
            return
        path = os.path.join(output_dir, file_name)
        self._forcibly_create_text_file_at_path_with_contents(path, contents)

    def write_header(self, output_dir):
        self._write_file(output_dir, self.generate_header, self.class_name + '.h')

    def write_headers_header(self, output_dir):
        self._write_file(output_dir, self.generate_headers_header, self.class_name + 'Headers.h')

    def write_interfaces_header(self, output_dir):
        self._write_file(output_dir, self.generate_interfaces_header, self.class_name + 'Interfaces.h')

    def write_implmentation(self, output_dir):
        self._write_file(output_dir, self.generate_implementation, self.class_name + '.cpp')

    def write_idl(self, output_dir):
        self._write_file(output_dir, self.generate_idl, self.class_name + '.idl')


class Maker(object):
    def __init__(self, writer_class):
        self._writer_class = writer_class

    def main(self, argv):
        script_name = os.path.basename(argv[0])
        args = argv[1:]
        if len(args) < 1:
            print "USAGE: %i INPUT_FILE [OUTPUT_DIRECTORY]" % script_name
            exit(1)
        output_dir = args[1] if len(args) > 1 else os.getcwd()

        writer = self._writer_class(args[0])
        writer.write_header(output_dir)
        writer.write_headers_header(output_dir)
        writer.write_interfaces_header(output_dir)
        writer.write_implmentation(output_dir)
        writer.write_idl(output_dir)
