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

# Should be a dict from 'sanitizer type' to 'compiler flag'.
SUPPORTED_SANITIZERS = {'asan': 'address'}


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
  build_dependencies = [l.strip() for l in command_result.stdout]
  return build_dependencies


def download_build_install(parsed_arguments):
  sanitizer_flag = SUPPORTED_SANITIZERS[parsed_arguments.sanitizer_type]
  
  environment = os.environ.copy()
  environment['CFLAGS'] = '-fsanitize=%s -g -fPIC -w' % sanitizer_flag
  environment['CXXFLAGS'] = '-fsanitize=%s -g -fPIC -w' % sanitizer_flag
  # We use XORIGIN as RPATH and after building library replace it to $ORIGIN
  # The reason: this flag goes through configure script and makefiles
  # differently for different libraries. So the dollar sign '$' should be
  # differently escaped. Instead of having problems with that it just
  # uses XORIGIN to build library and after that replaces it to $ORIGIN
  # directly in .so file.
  environment['LDFLAGS'] = '-Wl,-z,origin -Wl,-R,XORIGIN/.'

  library_directory = '%s/%s' % (parsed_arguments.intermediate_directory,
      parsed_arguments.library)
  
  install_prefix = '%s/%s/instrumented_libraries/%s' % (
      get_script_absolute_path(),
      parsed_arguments.product_directory,
      parsed_arguments.sanitizer_type)

  if not os.path.exists(library_directory):
    os.makedirs(library_directory)

  with ScopedChangeDirectory(library_directory), \
      open(os.devnull, 'w') as dev_null:
    if subprocess.call('apt-get source %s' % parsed_arguments.library,
        stdout=dev_null, stderr=dev_null, shell=True):
      raise Exception('Failed to download %s' % parsed_arguments.library)
    # There should be exactly one subdirectory after downloading a package.
    subdirectories = [d for d in os.listdir('.') if os.path.isdir(d)]
    if len(subdirectories) != 1:
      raise Exception('There was not one directory after downloading ' \
          'a package %s' % parsed_arguments.library)
    with ScopedChangeDirectory(subdirectories[0]):
      # Now we are in the package directory.
      configure_command = './configure %s --prefix=%s' % (
          parsed_arguments.custom_configure_flags, install_prefix)
      if subprocess.call(configure_command, stdout=dev_null, stderr=dev_null,
          env=environment, shell=True):
        raise Exception("Failed to configure %s" % parsed_arguments.library)
      if subprocess.call('make -j%s' % parsed_arguments.jobs,
          stdout=dev_null, stderr=dev_null, shell=True):
        raise Exception("Failed to make %s" % parsed_arguments.library)
      if subprocess.call('make -j%s install' % parsed_arguments.jobs,
          stdout=dev_null, stderr=dev_null, shell=True):
        raise Exception("Failed to install %s" % parsed_arguments.library)
  
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
  
  parsed_arguments = argument_parser.parse_args()
  # Ensure current working directory is this script directory
  os.chdir(get_script_absolute_path())
  # Ensure all build dependencies are installed
  build_dependencies = get_library_build_dependencies(parsed_arguments.library)
  if len(build_dependencies):
    print >> sys.stderr, 'Please, install build-dependencies for %s' % \
        parsed_arguments.library
    print >> sys.stderr, 'One-liner for APT:'
    print >> sys.stderr, 'sudo apt-get -y --no-remove build-dep %s' % \
        parsed_arguments.library
    sys.exit(1)

  download_build_install(parsed_arguments)


if __name__ == '__main__':
  main()
