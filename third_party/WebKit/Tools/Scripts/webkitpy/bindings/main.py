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

PASS_MESSAGE = 'All tests PASS!'
FAIL_MESSAGE = """Some tests FAIL!
To update the reference files, execute:
    run-bindings-tests --reset-results

If the failures are not due to your changes, test results may be out of sync;
please rebaseline them in a separate CL, after checking that tests fail in ToT.
In CL, please set:
NOTRY=true
TBR=(someone in Source/bindings/OWNERS or WATCHLISTS:bindings)
"""

# Python compiler is incomplete; skip IDLs with unimplemented features
SKIP_PYTHON = set([
    'TestCustomAccessors.idl',
    'TestEventTarget.idl',
    'TestException.idl',
    'TestImplements.idl',
    'TestInterface.idl',
    'TestInterfaceImplementedAs.idl',
    'TestNamedConstructor.idl',
    'TestNode.idl',
    'TestObject.idl',
    'TestOverloadedConstructors.idl',
    'TestPartialInterface.idl',
    'TestTypedefs.idl',
])

input_directory = os.path.join('bindings', 'tests', 'idls')
support_input_directory = os.path.join('bindings', 'tests', 'idls', 'testing')
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
    def __init__(self, reset_results, test_python, verbose, executive):
        self.reset_results = reset_results
        self.test_python = test_python
        self.verbose = verbose
        self.executive = executive
        _, self.interface_dependencies_filename = provider.newtempfile()
        _, self.derived_sources_list_filename = provider.newtempfile()
        # Generate output into the reference directory if resetting results, or
        # a temp directory if not.
        if reset_results:
            self.output_directory = reference_directory
        else:
            self.output_directory = provider.newtempdir()
        self.output_directory_py = provider.newtempdir()
        self.event_names_filename = os.path.join(self.output_directory, 'EventInterfaces.in')

    def run_command(self, cmd):
        output = self.executive.run_command(cmd)
        if output:
            print output

    def generate_from_idl_pl(self, idl_file):
        cmd = ['perl', '-w',
               '-Ibindings/scripts',
               '-Ibuild/scripts',
               '-Icore/scripts',
               '-I../../JSON/out/lib/perl5',
               'bindings/scripts/generate_bindings.pl',
               # idl include directories (path relative to generate-bindings.pl)
               '--include', '.',
               '--outputDir', self.output_directory,
               '--interfaceDependenciesFile', self.interface_dependencies_filename,
               '--idlAttributesFile', 'bindings/IDLExtendedAttributes.txt',
               idl_file]
        try:
            self.run_command(cmd)
        except ScriptError, e:
            print 'ERROR: generate_bindings.pl: ' + os.path.basename(idl_file)
            print e.output
            return e.exit_code
        return 0

    def generate_from_idl_py(self, idl_file):
        cmd = ['python',
               'bindings/scripts/unstable/idl_compiler.py',
               '--output-dir', self.output_directory_py,
               '--idl-attributes-file', 'bindings/IDLExtendedAttributes.txt',
               '--include', '.',
               '--interface-dependencies-file',
               self.interface_dependencies_filename,
               idl_file]
        try:
            self.run_command(cmd)
        except ScriptError, e:
            print 'ERROR: idl_compiler.py: ' + os.path.basename(idl_file)
            print e.output
            return e.exit_code
        return 0

    def generate_interface_dependencies(self):
        idl_files_list_file, main_idl_files_list_filename = provider.newtempfile()
        idl_paths = [os.path.join(input_directory, input_file)
                     for input_file in os.listdir(input_directory)
                     if input_file.endswith('.idl')]
        idl_files_list_contents = ''.join(idl_path + '\n'
                                          for idl_path in idl_paths)
        os.write(idl_files_list_file, idl_files_list_contents)
        support_idl_files_list_file, support_idl_files_list_filename = provider.newtempfile()
        support_idl_paths = [os.path.join(support_input_directory, input_file)
                     for input_file in os.listdir(support_input_directory)
                     if input_file.endswith('.idl')]
        support_idl_files_list_contents = ''.join(idl_path + '\n'
                                          for idl_path in support_idl_paths)
        os.write(support_idl_files_list_file, support_idl_files_list_contents)

        # Dummy files, required by compute_dependencies but not checked
        _, window_constructors_file = provider.newtempfile()
        _, workerglobalscope_constructors_file = provider.newtempfile()
        _, sharedworkerglobalscope_constructors_file = provider.newtempfile()
        _, dedicatedworkerglobalscope_constructors_file = provider.newtempfile()
        _, serviceworkersglobalscope_constructors_file = provider.newtempfile()
        cmd = ['python',
               'bindings/scripts/compute_dependencies.py',
               '--main-idl-files-list', main_idl_files_list_filename,
               '--support-idl-files-list', support_idl_files_list_filename,
               '--interface-dependencies-file', self.interface_dependencies_filename,
               '--bindings-derived-sources-file', self.derived_sources_list_filename,
               '--window-constructors-file', window_constructors_file,
               '--workerglobalscope-constructors-file', workerglobalscope_constructors_file,
               '--sharedworkerglobalscope-constructors-file', sharedworkerglobalscope_constructors_file,
               '--dedicatedworkerglobalscope-constructors-file', dedicatedworkerglobalscope_constructors_file,
               '--serviceworkerglobalscope-constructors-file', serviceworkersglobalscope_constructors_file,
               '--event-names-file', self.event_names_filename,
               '--write-file-only-if-changed', '0']

        if self.reset_results and self.verbose:
            print 'Reset results: EventInterfaces.in'
        try:
            self.run_command(cmd)
        except ScriptError, e:
            print 'ERROR: compute_dependencies.py'
            print e.output
            return e.exit_code
        return 0

    def identical_file(self, reference_filename, output_filename):
        reference_basename = os.path.basename(reference_filename)
        cmd = ['diff',
               '-u',
               '-N',
               reference_filename,
               output_filename]
        try:
            self.run_command(cmd)
        except ScriptError, e:
            # run_command throws an exception on diff (b/c non-zero exit code)
            print 'FAIL: %s' % reference_basename
            print e.output
            return False

        if self.verbose:
            print 'PASS: %s' % reference_basename
        return True

    def identical_output_files(self, output_directory):
        file_pairs = [(os.path.join(reference_directory, output_file),
                       os.path.join(output_directory, output_file))
                      for output_file in os.listdir(output_directory)
                      # FIXME: add option to compiler to not generate tables
                      if output_file != 'parsetab.py']
        return all([self.identical_file(reference_filename, output_filename)
                    for (reference_filename, output_filename) in file_pairs])

    def no_excess_files(self):
        generated_files = set(os.listdir(self.output_directory))
        generated_files.add('.svn')  # Subversion working copy directory
        excess_files = [output_file
                        for output_file in os.listdir(reference_directory)
                        if output_file not in generated_files]
        if excess_files:
            print ('Excess reference files! '
                  '(probably cruft from renaming or deleting):\n' +
                  '\n'.join(excess_files))
            return False
        return True

    def run_tests(self):
        # Generate output, immediately dying on failure
        if self.generate_interface_dependencies():
            return False

        for directory in [input_directory, support_input_directory]:
            for input_filename in os.listdir(directory):
                if not input_filename.endswith('.idl'):
                    continue
                idl_path = os.path.join(directory, input_filename)
                if self.generate_from_idl_pl(idl_path):
                    return False
                if self.reset_results and self.verbose:
                    print 'Reset results: %s' % input_filename
                if not self.test_python:
                    continue
                if (input_filename in SKIP_PYTHON or
                    directory == support_input_directory):
                    if self.verbose:
                        print 'SKIP: %s' % input_filename
                    continue
                if self.generate_from_idl_py(idl_path):
                    return False

        # Detect all changes
        passed = self.identical_file(reference_event_names_filename,
                                     self.event_names_filename)
        passed &= self.identical_output_files(self.output_directory)
        if self.test_python:
            if self.verbose:
                print
                print 'Python:'
            passed &= self.identical_output_files(self.output_directory_py)
        passed &= self.no_excess_files()
        return passed

    def main(self):
        current_scm = detect_scm_system(os.curdir)
        os.chdir(os.path.join(current_scm.checkout_root, 'Source'))

        all_tests_passed = self.run_tests()
        if all_tests_passed:
            if self.verbose:
                print
                print PASS_MESSAGE
            return 0
        print
        print FAIL_MESSAGE
        return -1
