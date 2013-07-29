# Copyright (C) 2011 Google Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

import os
import os.path
import shutil
import subprocess
import sys
import tempfile
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError


class ScopedTempFileProvider:
    def __init__(self):
        self.files = []

    def __del__(self):
        for f in self.files:
            os.remove(f)

    def newtempfile(self):
        path = tempfile.mkstemp()[1]
        self.files.append(path)
        return path


class BindingsTests:

    def __init__(self, reset_results, executive):
        self.reset_results = reset_results
        self.executive = executive

    def generate_from_idl(self, idl_file, output_directory, interface_dependencies_file):
        cmd = ['perl', '-w',
               '-Ibindings/scripts',
               '-Icore/scripts',
               '-I../../JSON/out/lib/perl5',
               'bindings/scripts/deprecated_generate_bindings.pl',
               # idl include directories (path relative to generate-bindings.pl)
               '--include', '.',
               '--outputDir', output_directory,
               '--interfaceDependenciesFile', interface_dependencies_file,
               '--idlAttributesFile', 'bindings/scripts/IDLAttributes.txt',
               idl_file]

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
            if output:
                print output
        except ScriptError, e:
            print e.output
            exit_code = e.exit_code
        return exit_code

    def generate_interface_dependencies(self, input_directory, interface_dependencies_file, window_constructors_file, workerglobalscope_constructors_file, sharedworkerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, event_names_file):
        idl_files_list = tempfile.mkstemp()
        for input_file in os.listdir(input_directory):
            (name, extension) = os.path.splitext(input_file)
            if extension != '.idl':
                continue
            os.write(idl_files_list[0], os.path.join(input_directory, input_file) + "\n")
        os.close(idl_files_list[0])

        cmd = ['python',
               'bindings/scripts/compute_dependencies.py',
               '--idl-files-list', idl_files_list[1],
               '--interface-dependencies-file', interface_dependencies_file,
               '--window-constructors-file', window_constructors_file,
               '--workerglobalscope-constructors-file', workerglobalscope_constructors_file,
               '--sharedworkerglobalscope-constructors-file', sharedworkerglobalscope_constructors_file,
               '--dedicatedworkerglobalscope-constructors-file', dedicatedworkerglobalscope_constructors_file,
               '--event-names-file', event_names_file,
               '--write-file-only-if-changed', '0']

        if self.reset_results:
            print "Reset results: EventInterfaces.in"

        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
            if output:
                print output
        except ScriptError, e:
            print e.output
            exit_code = e.exit_code
        os.remove(idl_files_list[1])
        return exit_code

    def detect_changes_in_file(self, work_file, reference_file):
        cmd = ['diff',
               '-u',
               '-N',
               os.path.join(reference_file),
               os.path.join(work_file)]

        changes_found = False
        exit_code = 0
        try:
            output = self.executive.run_command(cmd)
        except ScriptError, e:
            output = e.output
            exit_code = e.exit_code

        if exit_code or output:
            print 'FAIL: %s' % (os.path.basename(reference_file))
            print output
            changes_found = True
        else:
            print 'PASS: %s' % (os.path.basename(reference_file))

        return changes_found


    def detect_changes(self, work_directory, reference_directory):
        changes_found = False
        for output_file in os.listdir(work_directory):
            if self.detect_changes_in_file(os.path.join(reference_directory, output_file), os.path.join(work_directory, output_file)):
                changes_found = True
        return changes_found

    def run_tests(self, input_directory, reference_directory, interface_dependencies_file, event_names_file):
        work_directory = reference_directory

        passed = True

        if not self.reset_results and self.detect_changes_in_file(event_names_file, os.path.join(reference_directory, 'EventInterfaces.in')):
            passed = False

        for input_file in os.listdir(input_directory):
            (name, extension) = os.path.splitext(input_file)
            if extension != '.idl':
                continue
            # Generate output into the work directory (either the given one or a
            # temp one if not reset_results is performed)
            if not self.reset_results:
                work_directory = tempfile.mkdtemp()

            if self.generate_from_idl(os.path.join(input_directory, input_file),
                                      work_directory,
                                      interface_dependencies_file):
                passed = False

            if self.reset_results:
                print "Reset results: %s" % (input_file)
                continue

            # Detect changes
            if self.detect_changes(work_directory, reference_directory):
                passed = False
            shutil.rmtree(work_directory)

        return passed

    def main(self):
        current_scm = detect_scm_system(os.curdir)
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        all_tests_passed = True

        provider = ScopedTempFileProvider()

        input_directory = os.path.join('bindings', 'tests', 'idls')
        reference_directory = os.path.join('bindings', 'tests', 'results')

        interface_dependencies_file = provider.newtempfile()
        window_constructors_file = provider.newtempfile()
        workerglobalscope_constructors_file = provider.newtempfile()
        sharedworkerglobalscope_constructors_file = provider.newtempfile()
        dedicatedworkerglobalscope_constructors_file = provider.newtempfile()

        if self.reset_results:
            event_names_file = os.path.join(reference_directory, 'EventInterfaces.in')
        else:
            event_names_file = provider.newtempfile()

        if self.generate_interface_dependencies(input_directory, interface_dependencies_file, window_constructors_file, workerglobalscope_constructors_file, sharedworkerglobalscope_constructors_file, dedicatedworkerglobalscope_constructors_file, event_names_file):
            print 'Failed to generate interface dependencies file.'
            return -1

        if not self.run_tests(input_directory, reference_directory, interface_dependencies_file, event_names_file):
            all_tests_passed = False

        print ''
        if all_tests_passed:
            print 'All tests PASS!'
            return 0
        else:
            print 'Some tests FAIL! (To update the reference files, execute "run-bindings-tests --reset-results")'
            return -1
