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

import filecmp
import fnmatch
import os
import shutil
import sys
import tempfile

from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.system import executive

# Add Source path to PYTHONPATH to support function calls to bindings/scripts
# for compute_interfaces_info and idl_compiler
module_path = os.path.dirname(__file__)
source_path = os.path.normpath(os.path.join(module_path, os.pardir, os.pardir,
                                            os.pardir, os.pardir, 'Source'))
sys.path.append(source_path)

from bindings.scripts.compute_interfaces_info import compute_interfaces_info, interfaces_info
from bindings.scripts.idl_compiler import IdlCompilerV8


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

DEPENDENCY_IDL_FILES = frozenset([
    'TestImplements.idl',
    'TestImplements2.idl',
    'TestImplements3.idl',
    'TestPartialInterface.idl',
    'TestPartialInterface2.idl',
])


EXTENDED_ATTRIBUTES_FILE = 'bindings/IDLExtendedAttributes.txt'

all_input_directory = '.'  # Relative to Source/
test_input_directory = os.path.join('bindings', 'tests', 'idls')
reference_directory = os.path.join('bindings', 'tests', 'results')


class ScopedTempDir(object):
    """Wrapper for tempfile.mkdtemp() so it's usable with 'with' statement."""
    def __init__(self):
        self.dir_path = tempfile.mkdtemp()

    def __enter__(self):
        return self.dir_path

    def __exit__(self, exc_type, exc_value, traceback):
        # The temporary directory is used as an output directory, so it
        # contains unknown files (it isn't empty), hence use rmtree
        shutil.rmtree(self.dir_path)



class BindingsTests(object):
    def __init__(self, output_directory, verbose):
        self.verbose = verbose
        self.executive = executive.Executive()
        self.idl_compiler = None
        self.output_directory = output_directory

    def diff(self, filename1, filename2):
        # Python's difflib module is too slow, especially on long output, so
        # run external diff(1) command
        cmd = ['diff',
               '-u',
               '-N',
               filename1,
               filename2]
        # Return output and don't raise exception, even though diff(1) has
        # non-zero exit if files differ.
        return self.executive.run_command(cmd, error_handler=lambda x: None)

    def generate_from_idl(self, idl_file):
        idl_file_fullpath = os.path.realpath(idl_file)
        self.idl_compiler.compile_file(idl_file_fullpath)

    def generate_interface_dependencies(self):
        def idl_paths_recursive(directory):
            idl_paths = []
            for dirpath, _, files in os.walk(directory):
                idl_paths.extend(os.path.join(dirpath, filename)
                                 for filename in fnmatch.filter(files, '*.idl'))
            return idl_paths

        # We compute interfaces info for *all* IDL files, not just test IDL
        # files, as code generator output depends on inheritance (both ancestor
        # chain and inherited extended attributes), and some real interfaces
        # are special-cased, such as Node.
        #
        # For example, when testing the behavior of interfaces that inherit
        # from Node, we also need to know that these inherit from EventTarget,
        # since this is also special-cased and Node inherits from EventTarget,
        # but this inheritance information requires computing dependencies for
        # the real Node.idl file.
        compute_interfaces_info(idl_paths_recursive(all_input_directory))

    def delete_cache_files(self):
        # FIXME: Instead of deleting cache files, don't generate them.
        cache_files = [os.path.join(self.output_directory, output_file)
                       for output_file in os.listdir(self.output_directory)
                       if (output_file in ('lextab.py',  # PLY lex
                                           'lextab.pyc',
                                           'parsetab.pickle') or  # PLY yacc
                               output_file.endswith('.cache'))]  # Jinja
        for cache_file in cache_files:
            os.remove(cache_file)

    def identical_file(self, reference_filename, output_filename):
        reference_basename = os.path.basename(reference_filename)

        if not filecmp.cmp(reference_filename, output_filename):
            print 'FAIL: %s' % reference_basename
            print self.diff(reference_filename, output_filename)
            return False

        if self.verbose:
            print 'PASS: %s' % reference_basename
        return True

    def identical_output_files(self):
        file_pairs = [(os.path.join(reference_directory, output_file),
                       os.path.join(self.output_directory, output_file))
                      for output_file in os.listdir(self.output_directory)]
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
        self.generate_interface_dependencies()
        self.idl_compiler = IdlCompilerV8(self.output_directory,
                                          EXTENDED_ATTRIBUTES_FILE,
                                          interfaces_info=interfaces_info,
                                          only_if_changed=True)

        for input_filename in os.listdir(test_input_directory):
            if not input_filename.endswith('.idl'):
                continue
            if input_filename in DEPENDENCY_IDL_FILES:
                # Dependencies aren't built (they are used by the dependent)
                if self.verbose:
                    print 'DEPENDENCY: %s' % input_filename
                continue

            idl_path = os.path.join(test_input_directory, input_filename)
            self.generate_from_idl(idl_path)
            if self.verbose:
                print 'Compiled: %s' % input_filename

        self.delete_cache_files()

        # Detect all changes
        passed = self.identical_output_files()
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
        return 1


def run_bindings_tests(reset_results, verbose):
    # Generate output into the reference directory if resetting results, or
    # a temp directory if not.
    if reset_results:
        print 'Resetting results'
        return BindingsTests(reference_directory, verbose).main()
    with ScopedTempDir() as temp_dir:
        return BindingsTests(temp_dir, verbose).main()
