#! /usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Script to update .scons rules from the ppapi.gyp file.

This script reads the .scons files in this directory.  It replaces all of the
lines between the starting marker and the ending marker with the corresponding
list of files from the given gyp file.

The starting marker format is:
  # From GYP_FILE_NAME:TARGET:REGEXP
The ending marker format is:
  # End GYP_FILE_NAME

For example, if this exists in the .scons file:
  # From ppapi.gyp:ppapi_c:.*\.h
  ...
  # End ppapi.gyp

then this script will remove all of the lines between the starting marker and
the ending marker.  It will then find the 'ppapi_c' target in the ppapi.gyp
file.  It will find all 'sources' for that target that match the regular
expression '.*\.h' and will insert each of those source files in between the
two markers.

This script performs a similar action for the ppapi include tests.  The files
../../../tests/ppapi/cpp_header_test.cc and
../../../tests/ppapi/cpp_dev_header_test.cc have their include statements
replaced based on an include marker with format:
  // From ppapi.gyp:TARGET:REGEXP
"""

from optparse import OptionParser
import os
import re
import sys

# Constants.
PROG = os.path.basename(sys.argv[0])
NATIVE_CLIENT_SRC_SHARED_PPAPI = os.path.abspath(os.path.dirname(sys.argv[0]))
NATIVE_CLIENT_SRC_SHARED = os.path.dirname(NATIVE_CLIENT_SRC_SHARED_PPAPI)
NATIVE_CLIENT_SRC = os.path.dirname(NATIVE_CLIENT_SRC_SHARED)
NATIVE_CLIENT = os.path.dirname(NATIVE_CLIENT_SRC)
CLIENTDIR = os.path.dirname(NATIVE_CLIENT)

# Known files names.
PPAPI_GYP_DIR = os.path.join(CLIENTDIR, 'ppapi')
BUILD_SCONS = os.path.join(NATIVE_CLIENT_SRC_SHARED_PPAPI, 'build.scons')
NACL_SCONS = os.path.join(NATIVE_CLIENT_SRC_SHARED_PPAPI, 'nacl.scons')
PPAPI_GYP = os.path.join(NATIVE_CLIENT_SRC_SHARED_PPAPI, 'ppapi.gyp')
CPP_HEADER_TEST = os.path.join(
    NATIVE_CLIENT, 'tests', 'ppapi', 'cpp_header_test.cc')
CPP_DEV_HEADER_TEST = os.path.join(
    NATIVE_CLIENT, 'tests', 'ppapi', 'cpp_dev_header_test.cc')

# Regular expressions for the .scons files.
START_PATTERN = re.compile(
    '^([ \t]*)#[ \t]*From ([^:]*):([^:]*):(.*) *\n',
    re.IGNORECASE)
END_PATTERN = re.compile('^[ \t]*#[ \t]*End ([^:]*) *\n', re.IGNORECASE)

# Regular expressions for the .cc files.
INCLUDE_PATTERN = re.compile(
    '^[ \t]*//[ \t]*From ([^:]*):([^:]*):(.*) *\n',
    re.IGNORECASE)

# Regular expressions for the ppapi.gyp file.
TARGET_PATTERN = re.compile(
    '^[ \t]*\'target_name\'[ \t]*:[ \t]*\'([^\']*)\'[ \t]*,')
SOURCES_PATTERN = re.compile('^[ \t]*\'sources\'[ \t]*:[ \t]*[[]')
END_SOURCES_PATTERN = re.compile('^[ \t]*[]]')


def ParseCommandLine():
  """Parses options from the command line.

  Parses options from the command line and returns the options and the
  arguments to the caller.

  Returns:
    options: the options settings from the command line.
    args: the arguments (things that were not options).
  """
  usage = '%prog [options]'
  description = ('Update the .scons files in the "%s" directory with the'
                 ' current definitions from ppapi.gyp. Also update the #include'
                 ' test files in the "%s/../../../tests/ppapi" directory'
                 % (NATIVE_CLIENT_SRC_SHARED_PPAPI,
                    NATIVE_CLIENT_SRC_SHARED_PPAPI))
  parser = OptionParser(usage=usage, description=description)
  (options, args) = parser.parse_args()
  return (options, args)


def CheckFileIsReadable(filename):
  """Check the read accessibility of filename.

  Args:
    filename: the file to check.

  Returns:
    Exits to the system if the file is not readable.
  """
  if not os.access(filename, os.F_OK):
    print >> sys.stderr, '%s does not exist.' % (filename)
    sys.exit(1)
  if not os.access(filename, os.R_OK):
    print >> sys.stderr, 'Cannot read from %s' % (filename)
    sys.exit(1)


def CheckFileIsWritable(filename):
  """Check the write accessibility of filename.

  Args:
    filename: the file to check.

  Returns:
    Exits to the system if the file is not writable.
  """
  if os.access(filename, os.F_OK):
    # The file already exists. Check if it is writable.
    if not os.access(filename, os.W_OK):
      print >> sys.stderr, 'Cannot write to %s' % (filename)
      sys.exit(1)
  else:
    # The file does not yet exist. Check the directory.
    dirname = os.path.dirname(filename)
    if not dirname:
      dirname = '.'
    if not os.access(dirname, os.W_OK):
      print >> sys.stderr, 'Cannot write to directory %s' % (dirname)
      sys.exit(1)


def BuildTmpFilename(filename):
  """Returns the name of a temporary file for filename.

  Args:
    filename: the name of a file.

  Returns:
    The name of a temporary file.
  """
  return filename + '.tmp'


def RenameTmpToFile(filename):
  """Renames filename.tmp to filename.

  Args:
    filename: the final name of the file.
  """
  tmp_filename = BuildTmpFilename(filename)
  print '%s: Renaming %s back to %s' % (PROG, tmp_filename, filename)
  os.remove(filename)
  os.rename(tmp_filename, filename)


def TransferLinesFromPpapiGyp(marker, write_fp, is_cc_file):
  """Copies matching file lines from ppapi.gyp.

  Opens and reads ppapi.gyp to find the 'target' section that matches the
  target listed on the marker line.  Copies all of the files that match the
  marker's regular expression into the output file.

  Note that this function reads and parses the contents of ppapi.gyp each
  time it is called. This entire script should take less than 1 second to
  run, so the increased complexity involved in pre-caching the contents of
  the file and passing it around is unnecessary.

  Args:
    marker: The marker line from the file; matches START_PATTERN.
    write_fp: The file to write to.
    is_cc_file: False if this is a .scons file, True if .cc.
  """
  # Pluck off the interesting parts of the marker.
  if not is_cc_file:
    match = re.match(START_PATTERN, marker)
    scons_indentation = match.group(1)
    gyp_file_name = match.group(2)
    target = match.group(3)
    file_regexp = match.group(4)
  else:
    match = re.match(INCLUDE_PATTERN, marker)
    gyp_file_name = match.group(1)
    target = match.group(2)
    file_regexp = match.group(3)

  # Convert the input file_regexp into a pattern that will match the syntax
  # of a source file in the ppapi.gyp file.  The .gyp file looks like:
  #    'path/some-file.cc',
  # Put everything except the leading spaces into match.group(1).
  # So if the marker line in the .scons file contains the regexp:
  #    .*\.cc
  # the file_pattern regular expression (for .scons) will look like:
  #    ^ *('.*\.cc',)
  # and the file_pattern regular expression (for .cc) will look like:
  #    ^ *'(.*\.cc)',
  if not is_cc_file:
    file_pattern = re.compile('^[ \t]*(\'' + file_regexp + '\',)')
  else:
    file_pattern = re.compile('^[ \t]*\'(' + file_regexp + ')\',')

  gyp_file = open(os.path.join(PPAPI_GYP_DIR, gyp_file_name), 'r')
  found_target = False
  found_sources = False
  for line in gyp_file:
    if not found_target:
      # Still looking for:
      #    'target_name': 'TARGET'
      match = re.match(TARGET_PATTERN, line)
      if match:
        if match.group(1) == target:
          found_target = True
    elif not found_sources:
      # Looking for the start of the sources section for this target:
      #    'sources': [
      if re.match(SOURCES_PATTERN, line):
        found_sources = True
    elif re.match(END_SOURCES_PATTERN, line):
      # Found the ']' at the end of the 'sources': section.
      break
    else:
      # This is a line from the 'sources' section.  Does it match the filespec?
      match = re.match(file_pattern, line)
      if match:
        # Change the line's indentation to match the .scons file then write
        # the line to the .scons file.
        if not is_cc_file:
          out_line = scons_indentation + match.group(1) + '\n'
        else:
          out_line = '#include "ppapi/' + match.group(1) + '"\n'
        write_fp.write(out_line)

  gyp_file.close()


def UpdateSconsToTmp(filename):
  """Updates the input .scons file, writing to filename.tmp.

  Updates all filename lines between the start and end header markers with
  the current values from gyp_file. Writes all output to a temporary file.

  Args:
    filename: the file to update.
  """
  tmp_filename = BuildTmpFilename(filename)
  print '%s: Updating %s to %s' % (PROG, filename, tmp_filename)

  read_fp = open(filename, 'r')
  write_fp = open(tmp_filename, 'w')

  skipping_lines = False
  for line in read_fp:
    if not skipping_lines:
      # Currently copying all lines until a START_PATTERN line is found.
      write_fp.write(line)
      if re.match(START_PATTERN, line):
        TransferLinesFromPpapiGyp(line, write_fp, False)
        skipping_lines = True
    else:
      # All of the most recent source files have been copied from the
      # ppapi.gyp file into the output file.  We are now skipping all lines
      # until an END_PATTERN line is found.
      if re.match(END_PATTERN, line):
        write_fp.write(line)
        skipping_lines = False

  read_fp.close()
  write_fp.close()


def UpdateCcToTmp(filename):
  """Updates the input .cc file, writing to filename.tmp.

  Updates all #include lines after the inclusion marker with the
  current values from gyp_file. Writes all output to a temporary file.

  Args:
    filename: the file to update.
  """
  tmp_filename = BuildTmpFilename(filename)
  print '%s: Updating %s to %s' % (PROG, filename, tmp_filename)

  read_fp = open(filename, 'r')
  write_fp = open(tmp_filename, 'w')

  for line in read_fp:
    # Currently copying all lines until a INCLUDE_PATTERN line is found.
    write_fp.write(line)
    if re.match(INCLUDE_PATTERN, line):
      TransferLinesFromPpapiGyp(line, write_fp, True)
      break

  read_fp.close()
  write_fp.close()


def main():
  ParseCommandLine()

  # Make sure all of the files are accessible.
  CheckFileIsReadable(BUILD_SCONS)
  CheckFileIsReadable(NACL_SCONS)
  CheckFileIsReadable(PPAPI_GYP)
  CheckFileIsReadable(CPP_HEADER_TEST)
  CheckFileIsReadable(CPP_DEV_HEADER_TEST)
  CheckFileIsWritable(BuildTmpFilename(BUILD_SCONS))
  CheckFileIsWritable(BuildTmpFilename(NACL_SCONS))
  CheckFileIsWritable(BuildTmpFilename(PPAPI_GYP))
  CheckFileIsWritable(BuildTmpFilename(CPP_HEADER_TEST))
  CheckFileIsWritable(BuildTmpFilename(CPP_DEV_HEADER_TEST))

  # Update each of the .scons files into temporary files.
  UpdateSconsToTmp(BUILD_SCONS)
  UpdateSconsToTmp(NACL_SCONS)

  # Update the .gyp file into temporary file.
  UpdateSconsToTmp(PPAPI_GYP)

  # Update each of the .cc files into temporary files.
  UpdateCcToTmp(CPP_HEADER_TEST)
  UpdateCcToTmp(CPP_DEV_HEADER_TEST)

  # Copy the temporary files back to the real files.
  RenameTmpToFile(BUILD_SCONS)
  RenameTmpToFile(NACL_SCONS)
  RenameTmpToFile(PPAPI_GYP)
  RenameTmpToFile(CPP_HEADER_TEST)
  RenameTmpToFile(CPP_DEV_HEADER_TEST)

  return 0


if __name__ == '__main__':
  sys.exit(main())
