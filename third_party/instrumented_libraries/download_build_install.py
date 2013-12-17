#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Downloads, builds (with instrumentation) and installs shared libraries."""

import argparse
import os
import shutil
import subprocess
import sys

# Build parameters for different sanitizers
SUPPORTED_SANITIZERS = {
  'asan': {
    'compiler_flags': '-fsanitize=address -gline-tables-only -fPIC -w',
    'linker_flags': '-fsanitize=address -Wl,-z,origin -Wl,-R,XORIGIN/.'
  },
  'msan': {
    'compiler_flags': '-fsanitize=memory -fsanitize-memory-track-origins '
                      '-gline-tables-only -fPIC -w',
    'linker_flags': '-fsanitize=memory -Wl,-z,origin -Wl,-R,XORIGIN/.'
  },
}


class ScopedChangeDirectory(object):
  """Changes current working directory and restores it back automatically."""
  def __init__(self, path):
    self.path = path
    self.old_path = ''

  def __enter__(self):
    self.old_path = os.getcwd()
    os.chdir(self.path)

  def __exit__(self, exc_type, exc_value, traceback):
    os.chdir(self.old_path)


def get_script_absolute_path():
  return os.path.dirname(os.path.abspath(__file__))


def get_library_build_dependencies(library):
  command = 'apt-get -s build-dep %s | grep Inst | cut -d " " -f 2' % library
  command_result = subprocess.Popen(command, stdout=subprocess.PIPE,
      shell=True)
  if command_result.wait():
    raise Exception("Failed to determine build dependencies for %s" % library)
  build_dependencies = [l.strip() for l in command_result.stdout]
  return build_dependencies

def check_library_build_dependencies(library):
  build_dependencies = get_library_build_dependencies(library)
  if len(build_dependencies):
    print >> sys.stderr, 'Please, install build-dependencies for %s' % library
    print >> sys.stderr, 'One-liner for APT:'
    print >> sys.stderr, 'sudo apt-get -y --no-remove build-dep %s' % library
    sys.exit(1)

def shell_call(command, verbose=False, environment=None):
  """ Wrapper on subprocess.Popen
  
  Calls command with specific environment and verbosity using
  subprocess.Popen
  
  Args:
    command: Command to run in shell.
    verbose: If False, hides all stdout and stderr in case of successful build.
        Otherwise, always prints stdout and stderr.
    environment: Parameter 'env' for subprocess.Popen.

  Returns:
    None

  Raises:
    Exception: if return code after call is not zero.
  """
  child = subprocess.Popen(command, stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT, env=environment, shell=True)
  stdout, stderr = child.communicate()
  if verbose or child.returncode:
    print stdout
  if child.returncode:
    raise Exception("Failed to run: %s" % command)


def download_build_install(parsed_arguments):
  sanitizer_params = SUPPORTED_SANITIZERS[parsed_arguments.sanitizer_type]
  
  environment = os.environ.copy()
  # Usage of environment variables CC and CXX prefers usage flags --c-compiler
  # and --cxx-compiler
  if 'CC' not in environment and parsed_arguments.c_compiler:
    environment['CC'] = parsed_arguments.c_compiler
  if 'CXX' not in environment and parsed_arguments.cxx_compiler:
    environment['CXX'] = parsed_arguments.cxx_compiler
  environment['CFLAGS'] = sanitizer_params['compiler_flags']
  environment['CXXFLAGS'] = sanitizer_params['compiler_flags']
  # We use XORIGIN as RPATH and after building library replace it to $ORIGIN
  # The reason: this flag goes through configure script and makefiles
  # differently for different libraries. So the dollar sign '$' should be
  # differently escaped. Instead of having problems with that it just
  # uses XORIGIN to build library and after that replaces it to $ORIGIN
  # directly in .so file.
  environment['LDFLAGS'] = sanitizer_params['linker_flags']

  library_directory = '%s/%s' % (parsed_arguments.intermediate_directory,
      parsed_arguments.library)
  
  install_prefix = '%s/%s/instrumented_libraries/%s' % (
      get_script_absolute_path(),
      parsed_arguments.product_directory,
      parsed_arguments.sanitizer_type)

  if not os.path.exists(library_directory):
    os.makedirs(library_directory)

  with ScopedChangeDirectory(library_directory):
    shell_call('apt-get source %s' % parsed_arguments.library,
        parsed_arguments.verbose)
    # There should be exactly one subdirectory after downloading a package.
    subdirectories = [d for d in os.listdir('.') if os.path.isdir(d)]
    if len(subdirectories) != 1:
      raise (Exception('There was not one directory after downloading '
          'a package %s' % parsed_arguments.library))
    with ScopedChangeDirectory(subdirectories[0]):
      # Now we are in the package directory.
      configure_command = './configure %s --prefix=%s' % (
          parsed_arguments.custom_configure_flags, install_prefix)
      try:
        shell_call(configure_command, parsed_arguments.verbose, environment)
        shell_call('make -j%s' % parsed_arguments.jobs,
            parsed_arguments.verbose, environment)
        shell_call('make -j%s install' % parsed_arguments.jobs,
            parsed_arguments.verbose, environment)
      except Exception as exception:
        print exception
        print "Failed to build library %s." % parsed_arguments.library
        print ("Probably, some of its dependencies are not installed: %s" %
            ' '.join(get_library_build_dependencies(parsed_arguments.library)))
        sys.exit(1)

  # Touch a txt file to indicate library is installed.
  open('%s/%s.txt' % (install_prefix, parsed_arguments.library), 'w').close()

  # Remove downloaded package and generated temporary build files.
  shutil.rmtree(library_directory)


def main():
  argument_parser = argparse.ArgumentParser(
      description = 'Download, build and install instrumented library')
  
  argument_parser.add_argument('-j', '--jobs', type=int, default=1)
  argument_parser.add_argument('-l', '--library', required=True)
  argument_parser.add_argument('-i', '--product-directory', default='.',
      help='Relative path to the directory with chrome binaries')
  argument_parser.add_argument('-m', '--intermediate-directory', default='.',
      help='Relative path to the directory for temporary build files')
  argument_parser.add_argument('-c', '--custom-configure-flags', default='')
  argument_parser.add_argument('-s', '--sanitizer-type', required=True,
      choices=SUPPORTED_SANITIZERS.keys()) 
  argument_parser.add_argument('-v', '--verbose', action='store_true')
  argument_parser.add_argument('--check-build-deps', action='store_true')
  argument_parser.add_argument('--c-compiler')
  argument_parser.add_argument('--cxx-compiler')

  # Ignore all empty arguments because in several cases gyp passes them to the
  # script, but ArgumentParser treats them as positional arguments instead of
  # ignoring (and doesn't have such options).
  parsed_arguments = argument_parser.parse_args(
      [arg for arg in sys.argv[1:] if len(arg) != 0])
  # Ensure current working directory is this script directory.
  os.chdir(get_script_absolute_path())
  # Ensure all build dependencies are installed.
  if parsed_arguments.check_build_deps:
    check_library_build_dependencies(parsed_arguments.library)
    
  download_build_install(parsed_arguments)


if __name__ == '__main__':
  main()
