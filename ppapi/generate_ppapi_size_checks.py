#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script should be run manually on occasion to make sure all PPAPI types
have appropriate size checking.

"""

import optparse
import os
import subprocess
import sys


# The string that the PrintNamesAndSizes plugin uses to indicate a type is or
# contains a pointer.
HAS_POINTER_STRING = "HasPointer"
# These are types that don't include a pointer but nonetheless have
# architecture-dependent size.  They all are ultimately typedefs to 'long' or
# 'unsigned long'.  If there get to be too many types that use 'long' or
# 'unsigned long', we can add detection of them to the plugin to automate this.
ARCH_DEPENDENT_TYPES = set(["khronos_intptr_t",
                            "khronos_uintptr_t",
                            "khronos_ssize_t",
                            "khronos_usize_t",
                            "GLintptr",
                            "GLsizeiptr"
                           ])



class SourceLocation:

  """A class representing the source location of a definiton."""

  def __init__(self, filename="", start_line=-1, end_line=-1):
    self.filename = os.path.normpath(filename)
    self.start_line = start_line
    self.end_line = end_line



class TypeInfo:

  """A class representing information about a C++ type."""

  def __init__(self, info_string, target):
    [self.kind, self.name, self.size, has_pointer_string, source_file,
        start_line, end_line] = info_string.split(',')
    self.target = target
    self.parsed_line = info_string
    # Note that Clang counts line numbers from 1, but we want to count from 0.
    self.source_location = SourceLocation(source_file,
                                          int(start_line)-1,
                                          int(end_line)-1)
    # If the type is one of our known special cases, mark it as architecture-
    # dependent.
    if self.name in ARCH_DEPENDENT_TYPES:
      self.arch_dependent = True
    # Otherwise, its size can be architecture dependent if it contains one or
    # more pointers (or is a pointer).
    else:
      self.arch_dependent = (has_pointer_string == HAS_POINTER_STRING)
    self.target = target
    self.parsed_line = info_string



class FilePatch:

  """A class representing a set of line-by-line changes to a particular file.
  None
  of the changes are applied until Apply is called.  All line numbers are
  counted from 0.
  """

  def __init__(self, filename):
    self.filename = filename
    self.linenums_to_delete = set()
    # A dictionary from line number to an array of strings to be inserted at
    # that line number.
    self.lines_to_add = {}

  def Delete(self, start_line, end_line):
    """Make the patch delete the lines [start_line, end_line)."""
    self.linenums_to_delete |= set(range(start_line, end_line))

  def Add(self, text, line_number):
    """Add the given text before the text on the given line number."""
    if line_number in self.lines_to_add:
      self.lines_to_add[line_number].append(text)
    else:
      self.lines_to_add[line_number] = [text]

  def Apply(self):
    """Apply the patch by writing it to self.filename"""
    # Read the lines of the existing file in to a list.
    sourcefile = open(self.filename, "r")
    file_lines = sourcefile.readlines()
    sourcefile.close()
    # Now apply the patch.  Our strategy is to keep the array at the same size,
    # and just edit strings in the file_lines list as necessary.  When we delete
    # lines, we just blank the line and keep it in the list.  When we add lines,
    # we just prepend the added source code to the start of the existing line at
    # that line number.  This way, all the line numbers we cached from calls to
    # Add and Delete remain valid list indices, and we don't have to worry about
    # maintaining any offsets.  Each element of file_lines at the end may
    # contain any number of lines (0 or more) delimited by carriage returns.
    for linenum_to_delete in self.linenums_to_delete:
      file_lines[linenum_to_delete] = "";
    for linenum, sourcelines in self.lines_to_add.items():
      # Sort the lines we're adding so we get relatively consistent results.
      sourcelines.sort()
      # Prepend the new lines.  When we output
      file_lines[linenum] = "".join(sourcelines) + file_lines[linenum]
    newsource = open(self.filename, "w")
    for line in file_lines:
      newsource.write(line)
    newsource.close()


def CheckAndInsert(typeinfo, typeinfo_map):
  """Check if a TypeInfo exists already in the given map with the same name.  If
  so, make sure the size is consistent.
  - If the name exists but the sizes do not match, print a message and
    exit with non-zero exit code.
  - If the name exists and the sizes match, do nothing.
  - If the name does not exist, insert the typeinfo in to the map.

  """
  # If the type is unnamed, ignore it.
  if typeinfo.name == "":
    return
  # If the size is 0, ignore it.
  elif int(typeinfo.size) == 0:
    return
  # If the type is not defined under ppapi, ignore it.
  elif typeinfo.source_location.filename.find("ppapi") == -1:
    return
  # If the type is defined under GLES2, ignore it.
  elif typeinfo.source_location.filename.find("GLES2") > -1:
    return
  # If the type is an interface (by convention, starts with PPP_ or PPB_),
  # ignore it.
  elif (typeinfo.name[:4] == "PPP_") or (typeinfo.name[:4] == "PPB_"):
    return
  elif typeinfo.name in typeinfo_map:
    if typeinfo.size != typeinfo_map[typeinfo.name].size:
      print "Error: '" + typeinfo.name + "' is", \
          typeinfo_map[typeinfo.name].size, \
          "bytes on target '" + typeinfo_map[typeinfo.name].target + \
          "', but", typeinfo.size, "on target '" + typeinfo.target + "'"
      print typeinfo_map[typeinfo.name].parsed_line
      print typeinfo.parsed_line
      sys.exit(1)
    else:
      # It's already in the map and the sizes match.
      pass
  else:
    typeinfo_map[typeinfo.name] = typeinfo


def ProcessTarget(clang_command, target, arch_types, ind_types):
  """Run clang using the given clang_command for the given target string.  Parse
  the output to create TypeInfos for each discovered type.  Insert each type in
  to the appropriate dictionary.  For each type that has architecture-dependent
  size, insert it in to arch_types.  Types with independent size go in to
  ind_types.  If the type already exists in the appropriate map, make sure that
  the size matches what's already in the map.  If not, the script terminates
  with an error message.
  """
  p = subprocess.Popen(clang_command + " -triple " + target,
                       shell=True,
                       stdout=subprocess.PIPE)
  lines = p.communicate()[0].split()
  for line in lines:
    typeinfo = TypeInfo(line, target)
    # Put types which have architecture-specific size in to arch_types.  All
    # other types are 'architecture-independent' and get put in ind_types.
    # in the appropraite dictionary.
    if typeinfo.arch_dependent:
      CheckAndInsert(typeinfo, arch_types)
    else:
      CheckAndInsert(typeinfo, ind_types)


def ToAssertionCode(typeinfo):
  """Convert the TypeInfo to an appropriate C compile assertion.
  If it's a struct (Record in Clang terminology), we want a line like this:
    PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(<name>, <size>);\n
  Enums:
    PP_COMPILE_ASSERT_ENUM_SIZE_IN_BYTES(<name>, <size>);\n
  Typedefs:
    PP_COMPILE_ASSERT_SIZE_IN_BYTES(<name>, <size>);\n

  """
  line = "PP_COMPILE_ASSERT_"
  if typeinfo.kind == "Enum":
    line += "ENUM_"
  elif typeinfo.kind == "Record":
    line += "STRUCT_"
  line += "SIZE_IN_BYTES("
  line += typeinfo.name
  line += ", "
  line += typeinfo.size
  line += ");\n"
  return line


def IsMacroDefinedName(typename):
  """Return true iff the given typename came from a PPAPI compile assertion."""
  return typename.find("PP_Dummy_Struct_For_") == 0


COPYRIGHT_STRING_C = \
"""/* Copyright (c) 2010 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file has compile assertions for the sizes of types that are dependent
 * on the architecture for which they are compiled (i.e., 32-bit vs 64-bit).
 */

"""


def WriteArchSpecificCode(types, root, filename):
  """Write a header file that contains a compile-time assertion for the size of
     each of the given typeinfos, in to a file named filename rooted at root.
  """
  assertion_lines = [ToAssertionCode(typeinfo) for typeinfo in types]
  assertion_lines.sort()
  outfile = open(os.path.join(root, filename), "w")
  header_guard = "PPAPI_TESTS_" + filename.upper().replace(".", "_") + "_"
  outfile.write(COPYRIGHT_STRING_C)
  outfile.write('#ifndef ' + header_guard + '\n')
  outfile.write('#define ' + header_guard + '\n\n')
  outfile.write('#include "ppapi/tests/test_struct_sizes.c"\n\n')
  for line in assertion_lines:
    outfile.write(line)
  outfile.write('\n#endif  /* ' + header_guard + ' */\n')


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option(
      '-c', '--clang-path', dest='clang_path',
      default=(''),
      help='the path to the clang binary (default is to get it from your path)')
  parser.add_option(
      '-p', '--plugin', dest='plugin',
      default='tests/clang/libPrintNamesAndSizes.so',
      help='The path to the PrintNamesAndSizes plugin library.')
  parser.add_option(
      '--targets32', dest='targets32',
      default='i386-pc-linux,arm-pc-linux,i386-pc-win32',
      help='Which 32-bit target triples to provide to clang.')
  parser.add_option(
      '--targets64', dest='targets64',
      default='x86_64-pc-linux,x86_64-pc-win',
      help='Which 32-bit target triples to provide to clang.')
  parser.add_option(
      '-r', '--ppapi-root', dest='ppapi_root',
      default='.',
      help='The root directory of ppapi.')
  options, args = parser.parse_args(argv)
  if args:
    parser.print_help()
    print 'ERROR: invalid argument'
    sys.exit(1)

  clang_executable = os.path.join(options.clang_path, 'clang')
  clang_command = clang_executable + " -cc1" \
      + " -load " + options.plugin \
      + " -plugin PrintNamesAndSizes" \
      + " -I" + os.path.join(options.ppapi_root, "..") \
      + " " \
      + os.path.join(options.ppapi_root, "tests", "test_struct_sizes.c")

  # Dictionaries mapping type names to TypeInfo objects.
  # Types that have size dependent on architecture, for 32-bit
  types32 = {}
  # Types that have size dependent on architecture, for 64-bit
  types64 = {}
  # Note that types32 and types64 should contain the same types, but with
  # different sizes.

  # Types whose size should be consistent regardless of architecture.
  types_independent = {}

  # Now run clang for each target.  Along the way, make sure architecture-
  # dependent types are consistent sizes on all 32-bit platforms and consistent
  # on all 64-bit platforms.  Any types in 'types_independent' are checked for
  # all targets to make sure their size is consistent across all of them.
  targets32 = options.targets32.split(',');
  for target in targets32:
    ProcessTarget(clang_command, target, types32, types_independent)
  targets64 = options.targets64.split(',');
  for target in targets64:
    ProcessTarget(clang_command, target, types64, types_independent)

  # This dictionary maps file names to FilePatch objects.
  file_patches = {}

  # Find locations of existing macros, and just delete them all.
  for name, typeinfo in \
      types_independent.items() + types32.items() + types64.items():
    if IsMacroDefinedName(name):
      sourcefile = typeinfo.source_location.filename
      if sourcefile not in file_patches:
        file_patches[sourcefile] = FilePatch(sourcefile)
      file_patches[sourcefile].Delete(typeinfo.source_location.start_line,
                                      typeinfo.source_location.end_line+1)

  # Add a compile-time assertion for each type whose size is independent of
  # architecture.  These assertions go immediately after the class definition.
  for name, typeinfo in types_independent.items():
    # Ignore macros and types that are 0 bytes (i.e., typedefs to void)
    if not IsMacroDefinedName(name) and typeinfo.size > 0:
      sourcefile = typeinfo.source_location.filename
      if sourcefile not in file_patches:
        file_patches[sourcefile] = FilePatch(sourcefile)
      # Add the assertion code just after the definition of the type.
      file_patches[sourcefile].Add(ToAssertionCode(typeinfo),
                                   typeinfo.source_location.end_line+1)

  for filename, patch in file_patches.items():
    patch.Apply()

  c_source_root = os.path.join(options.ppapi_root, "tests")
  WriteArchSpecificCode(types32.values(),
                        c_source_root,
                        "arch_dependent_sizes_32.h")
  WriteArchSpecificCode(types64.values(),
                        c_source_root,
                        "arch_dependent_sizes_64.h")

  return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

