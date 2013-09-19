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
import shutil
import tempfile
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system.executive import ScriptError

# Python compiler is incomplete; skip IDLs with unimplemented features
SKIP_PYTHON = set([
    'TestActiveDOMObject.idl',
    'TestCallback.idl',
    'TestCustomAccessors.idl',
    'TestEvent.idl',
    'TestEventConstructor.idl',
    'TestEventTarget.idl',
    'TestException.idl',
    'TestExtendedEvent.idl',
    'TestImplements.idl',
    'TestInterface.idl',
    'TestInterfaceImplementedAs.idl',
    'TestMediaQueryListListener.idl',
    'TestNamedConstructor.idl',
    'TestNode.idl',
    'TestObject.idl',
    'TestOverloadedConstructors.idl',
    'TestPartialInterface.idl',
    'TestSerializedScriptValueInterface.idl',
    'TestTypedArray.idl',
    'TestTypedefs.idl',
])

input_directory = os.path.join('bindings', 'tests', 'idls')
reference_directory = os.path.join('bindings', 'tests', 'results')
reference_event_names_filename = os.path.join(reference_directory, 'EventInterfaces.in')


class ScopedTempFileProvider(object):
    def __init__(self):
        self.files = []
        self.directories = []

    def __del__(self):
        for filename in self.files:
            os.remove(filename)
        for directory in self.directories:
            shutil.rmtree(directory)

    def newtempfile(self):
        file_handle, path = tempfile.mkstemp()
        self.files.append(path)
        return file_handle, path

    def newtempdir(self):
        path = tempfile.mkdtemp()
        self.directories.append(path)
        return path

provider = ScopedTempFileProvider()


class BindingsTests(object):
    def __init__(self, reset_results, test_python, executive):
        self.reset_results = reset_results
        self.test_python = test_python
        self.executive = executive
        _, self.interface_dependencies_filename = provider.newtempfile()
        if reset_results:
            self.event_names_filename = os.path.join(reference_directory, 'EventInterfaces.in')
        else:
            _, self.event_names_filename = provider.newtempfile()

    def run_command(self, cmd):
        return self.executive.run_command(cmd)

    def generate_from_idl_pl(self, idl_file, output_directory):
        cmd = ['perl', '-w',
               '-Ibindings/scripts',
               '-Icore/scripts',
               '-I../../JSON/out/lib/perl5',
               'bindings/scripts/generate_bindings.pl',
               # idl include directories (path relative to generate-bindings.pl)
               '--include', '.',
               '--outputDir', output_directory,
               '--interfaceDependenciesFile', self.interface_dependencies_filename,
               '--idlAttributesFile', 'bindings/scripts/IDLAttributes.txt',
               idl_file]
        try:
            output = self.run_command(cmd)
        except ScriptError, e:
            print e.output
            return e.exit_code
        if output:
            print output
        return 0

    def generate_from_idl_py(self, idl_file, output_directory):
        cmd = ['python',
               'bindings/scripts/unstable/idl_compiler.py',
               '--output-dir', output_directory,
               '--idl-attributes-file', 'bindings/scripts/IDLAttributes.txt',
               '--include', '.',
               '--interface-dependencies-file',
               self.interface_dependencies_filename,
               idl_file]
        try:
            output = self.run_command(cmd)
        except ScriptError, e:
            print e.output
            return e.exit_code
        if output:
            print output
        return 0

    def generate_interface_dependencies(self):
        idl_files_list_file, idl_files_list_filename = provider.newtempfile()
        idl_paths = [os.path.join(input_directory, input_file)
                     for input_file in os.listdir(input_directory)
                     if input_file.endswith('.idl')]
        idl_files_list_contents = ''.join(idl_path + '\n'
                                          for idl_path in idl_paths)
        os.write(idl_files_list_file, idl_files_list_contents)

        # Dummy files, required by compute_dependencies but not checked
        _, window_constructors_file = provider.newtempfile()
        _, workerglobalscope_constructors_file = provider.newtempfile()
        _, sharedworkerglobalscope_constructors_file = provider.newtempfile()
        _, dedicatedworkerglobalscope_constructors_file = provider.newtempfile()
        cmd = ['python',
               'bindings/scripts/compute_dependencies.py',
               '--idl-files-list', idl_files_list_filename,
               '--interface-dependencies-file', self.interface_dependencies_filename,
               '--window-constructors-file', window_constructors_file,
               '--workerglobalscope-constructors-file', workerglobalscope_constructors_file,
               '--sharedworkerglobalscope-constructors-file', sharedworkerglobalscope_constructors_file,
               '--dedicatedworkerglobalscope-constructors-file', dedicatedworkerglobalscope_constructors_file,
               '--event-names-file', self.event_names_filename,
               '--write-file-only-if-changed', '0']

        if self.reset_results:
            print 'Reset results: EventInterfaces.in'
        try:
            output = self.run_command(cmd)
        except ScriptError, e:
            print e.output
            return e.exit_code
        if output:
            print output
        return 0

    def identical_file(self, reference_filename, work_filename):
        cmd = ['diff',
               '-u',
               '-N',
               reference_filename,
               work_filename]
        try:
            output = self.run_command(cmd)
        except ScriptError, e:
            print e.output
            return False

        reference_basename = os.path.basename(reference_filename)
        if output:
            print 'FAIL: %s' % reference_basename
            print output
            return False
        print 'PASS: %s' % reference_basename
        return True

    def identical_output_directory(self, work_directory):
        file_pairs = [(os.path.join(reference_directory, output_file),
                       os.path.join(work_directory, output_file))
                      for output_file in os.listdir(work_directory)
                      # FIXME: add option to compiler to not generate tables
                      if output_file != 'parsetab.py']
        return all([self.identical_file(reference_filename, work_filename)
                    for (reference_filename, work_filename) in file_pairs])

    def run_tests(self):
        def generate_and_check_output_pl(idl_filename):
            # Generate output into the reference directory if resetting
            # results, or a temp directory if not.
            if self.reset_results:
                work_directory = reference_directory
            else:
                work_directory = provider.newtempdir()
            idl_path = os.path.join(input_directory, idl_filename)
            if self.generate_from_idl_pl(idl_path, work_directory):
                return False
            if self.reset_results:
                print 'Reset results: %s' % input_file
                return True
            return self.identical_output_directory(work_directory)

        def generate_and_check_output_py(idl_filename):
            if idl_filename in SKIP_PYTHON:
                print 'SKIP: %s' % idl_filename
                return True
            work_directory = provider.newtempdir()
            idl_path = os.path.join(input_directory, idl_filename)
            if self.generate_from_idl_py(idl_path, work_directory):
                return False
            # Detect changes
            return self.identical_output_directory(work_directory)

        if self.reset_results:
            passed = True
        else:
            passed = self.identical_file(reference_event_names_filename,
                                         self.event_names_filename)
        passed &= all([generate_and_check_output_pl(input_file)
                       for input_file in os.listdir(input_directory)
                       if input_file.endswith('.idl')])
        print
        if self.test_python:
            print 'Python:'
            passed &= all([generate_and_check_output_py(input_file)
                           for input_file in os.listdir(input_directory)
                           if input_file.endswith('.idl')])
        return passed

    def main(self):
        current_scm = detect_scm_system(os.curdir)
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        if self.generate_interface_dependencies():
            print 'Failed to generate interface dependencies file.'
            return -1

        all_tests_passed = self.run_tests()
        print
        if all_tests_passed:
            print 'All tests PASS!'
            return 0
        print 'Some tests FAIL! (To update the reference files, execute "run-bindings-tests --reset-results")'
        return -1
