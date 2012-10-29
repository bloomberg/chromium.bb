#!/usr/bin/python2.4
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
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

""" Generates C header/source files for an enumerated type.

Usage: python enum_gen.py <enum_spec>.enum
"""

import getopt
import logging
import os
import re
import sys

logging.getLogger().setLevel(logging.INFO)

def _Usage():
  print >>sys.stderr, 'Usage: enum_gen [options] <enum_spec>.enum'

# Command line options, and thier default values.
_ClOptions = {
    # Header file to generate - default is <enum_spec>.h
    'header': None,
    # Source file to generate - default is <enum_spec>.c
    'source': None,
    # Name of enumerated type - default is <enum_spec>
    'name': None,
    # Prefix to remove print print names - default is None
    'name_prefix': None,
    # Path prefix to remove from descriptions of generated source.
    'path_prefix': None,
    # Sort enumerated constants
    'sort': 0,
    # Add a "EnumSize" constant at the end of the list.
    'add_size': 0
    }

# Pattern expected for enumerated values to generate.
_filename_form = re.compile(r'^(.*)\.enum$')

# Verify that the enumeration filename matches expectations.
def _ValidateFilename(filename):
  if not _filename_form.match(filename):
    logging.error("Enum argument of wrong form: %s", filename)
    sys.exit(1)

# Given the enumeration filename, return the corresponding header
# file that contains the enumeration and the name function declaration.
def _GetHeaderFilename(filename):
  # Use command line specification if defined.
  header = _ClOptions['header']
  if header is None:
    # Replace the .enum suffix of with .h
    for prefix in _filename_form.findall(filename):
      return prefix + ".h"
    # This should not be reached.
    logging.error("Unable to generate header file name: %s", filename)
    sys.exit(1)
  else:
    return header

# Given the enumeration filename, return the name of the enumerated type
# being defined.
def _GetEnumName(filename):
  # Use the command line specification if defined.
  name = _ClOptions['name']
  if name is None:
    # Use the filename without the suffix.
    for prefix in _filename_form.findall(filename):
      return prefix
    # This should not be reached.
    logging.error("Unable to generate enum name: %s", filename)
    sys.exit(1)
  else:
    return name

# Generate the enumeration constant name to use for the constant
# specified in the enumeration file.
def _GetPrintName(constant):
  # See if the command line specifies a prefix. If so, add it to the
  # constant name.
  prefix = _ClOptions['name_prefix']
  if prefix is None:
    return constant
  else:
    return prefix + constant

# Given the enumeration filename, return the name of the file containing
# the implementation of the enumeration.
def _GetSourceFilename(filename):
  # Use the comand line specification if defined.
  source = _ClOptions['source']
  if source is None:
    # Replace the .enum suffix with .c
    for prefix in _filename_form.findall(filename):
      return prefix + ".c"
    # This should not be reached.
    logging.error("Unable to generate source file name: %s", filename)
    sys.exit(1)
  else:
    return source

# Given a filename, remove the path_prefix if possible.
def _GetSimplifiedFilename(filename):
  # Use the path prefix specified on the command line.
  path_prefix = _ClOptions['path_prefix']
  if path_prefix is not None:
    if filename.startswith(path_prefix):
      filename = filename[len(path_prefix):]
  return filename

# Given the list of enumerated constants from the given enumeration file,
# update the constants based on command line options.
def _ApplyFilters(constants):
  if _ClOptions['sort']:
    constants.sort()
  return constants

# Given the enumeration file name, open the file and return the list of
# constants defined in the file.
def _ReadConstants(filename):
  name_pattern = re.compile(r'^(\w+)\s*\#?')
  infile = open(filename, "r")
  constants = []
  for line in infile:
    line = line.rstrip()
    trimmed_line = line.lstrip()
    if len(trimmed_line) > 0 and not trimmed_line.startswith("#"):
      if name_pattern.match(trimmed_line):
        for name in name_pattern.findall(trimmed_line):
          constants.append(name)
      else:
        logging.error("Line not understood: %s", line)
        sys.exit(1)
  if constants == []:
    logging.error("No enumerated values found")
    sys.exit(1)
  return constants

# Generate a DO NOT EDIT banner in the given file.
#    file - The file to put the banner
#    filename - The name of the file to put the banner in.
#    enumfile - The name of the enumeration file.
def _AddDoNotEditMessage(file, filename, enumfile):
  print >>file, "/* %s" % _GetSimplifiedFilename(filename)
  print >>file, " * THIS FILE IS AUTO_GENERATED DO NOT EDIT."
  print >>file, " *"
  print >>file, " * This file was auto-generated by enum_gen.py"
  print >>file, " * from file %s" % os.path.basename(enumfile)
  print >>file, " */"
  print >>file, ""

# Given a file name, convert it to the DEFINE name to use to make sure
# the file is included at most once.
def _GetDefineName(filename):
  return filename.replace(".", "_").replace(
      "/", "_").replace("\\", "_").replace("-","_").upper()

# Given the enumeration file name, and the constants defined within that file,
# Generate a header file defining the enumeration, and the corresponding function
# to print out symbolic names for each constant.
def _GenerateHeader(enumfile, constants):
  filename = _GetHeaderFilename(enumfile)
  outfile = open(filename, "w")
  simplified_filename = _GetSimplifiedFilename(filename)
  _AddDoNotEditMessage(outfile, filename, enumfile)
  enumname = _GetEnumName(enumfile)
  print >>outfile, "#ifndef %s__" % _GetDefineName(simplified_filename)
  print >>outfile, "#define %s__" % _GetDefineName(simplified_filename)
  print >>outfile, ""
  print >>outfile, '#include "native_client/src/include/portability.h"'
  print >>outfile, ""
  print >>outfile, "EXTERN_C_BEGIN"
  print >>outfile, "typedef enum %s {" % enumname
  enum_value = 0
  for constant in constants:
    print >>outfile, "  %s = %d," % (_GetPrintName(constant), enum_value)
    enum_value += 1
  if _ClOptions['add_size']:
    print >>outfile, ("  %sEnumSize = %d, " +
                      "/* special size marker */") % (enumname, enum_value)
  print >>outfile, "} %s;" % enumname
  print >>outfile, ""
  print >>outfile, "/* Returns the name of an %s constant. */" % enumname
  print >>outfile, "extern const char* %sName(%s name);" % (enumname, enumname)
  print >>outfile, ""
  print >>outfile, "EXTERN_C_END"
  print >>outfile, ""
  print >>outfile, "#endif /* %s__ */" % _GetDefineName(simplified_filename)

# Given the enumeration file name, and the constants defined within that file,
# Generate an implementation file defining the corresponding function to print
# out symbolic names for each constant.
def _GenerateSource(enumfile, constants):
  filename = _GetSourceFilename(enumfile)
  outfile = open(filename, "w")
  _AddDoNotEditMessage(outfile, filename, enumfile)
  enumname = _GetEnumName(enumfile)
  sizename = constants[-1]
  if _ClOptions['add_size']:
    sizename = enumname + 'EnumSize'
  print >>outfile, "/* Define the corresponding names of %s. */" % enumname
  print >>outfile, ("static const char* " +
                    "const g_%sName[%s + 1] = {") % (enumname, sizename)
  for constant in constants:
    print >>outfile, '  "%s",' % constant
  if _ClOptions['add_size']:
    print >>outfile, '  "%sEnumSize"' % enumname
  print >>outfile, "};"
  print >>outfile, ""
  print >>outfile, "const char* %sName(%s name) {" % (enumname, enumname)
  print >>outfile, "  return name <= %s" % sizename
  print >>outfile, "    ? g_%sName[name]" % enumname
  print >>outfile, '    : "%s???";' % enumname
  print >>outfile, "}"

# Given an array of command line arguments, process command line options, and
# return a list of arguments that aren't command line options.
def _ProcessOptions(argv):
  """Process command line options and return the unprocessed left overs."""
  try:
    opts, args = getopt.getopt(argv, '', [x + '='  for x in _ClOptions])
  except getopt.GetoptError, err:
    print(str(err))  # will print something like 'option -a not recognized'
    sys.exit(-1)

  for o, a in opts:
    # strip the leading '--'
    option = o[2:]
    assert option in _ClOptions
    if type(_ClOptions[option]) == int:
      _ClOptions[option] = int(a)
    else:
      _ClOptions[option] = a
  # return the unprocessed options, i.e. the command
  return args

# Given an enumeration file to generate, build the corresponding header/source
# files implementing the enumerated type.
def main(argv):
  command = _ProcessOptions(argv)
  if len(command) != 1:
    _Usage()
    return 1
  enumfile = command[0]
  _ValidateFilename(enumfile)
  constants = _ApplyFilters(_ReadConstants(enumfile))
  _GenerateHeader(enumfile, constants)
  _GenerateSource(enumfile, constants)
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
