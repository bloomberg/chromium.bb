# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Takes the same arguments as Windows link.exe, and a definition of libraries
to split into subcomponents. Does multiple passes of link.exe invocation to
determine exports between parts and generates .def and import libraries to
cause symbols to be available to other parts."""

import _winreg
import ctypes
import os
import re
import subprocess
import sys


BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def Log(message):
  print 'split_link:', message


def GetFlagsAndInputs(argv):
  """Parses the command line intended for link.exe and return the flags and
  input files."""
  rsp_expanded = []
  for arg in argv:
    if arg[0] == '@':
      with open(arg[1:]) as rsp:
        rsp_expanded.extend(rsp.read().splitlines())
    else:
      rsp_expanded.append(arg)

  # Use CommandLineToArgvW so we match link.exe parsing.
  try:
    size = ctypes.c_int()
    ptr = ctypes.windll.shell32.CommandLineToArgvW(
        ctypes.create_unicode_buffer(' '.join(rsp_expanded)),
        ctypes.byref(size))
    ref = ctypes.c_wchar_p * size.value
    raw = ref.from_address(ptr)
    args = [arg for arg in raw]
  finally:
    ctypes.windll.kernel32.LocalFree(ptr)

  inputs = []
  flags = []
  for arg in args:
    lower_arg = arg.lower()
    # We'll be replacing this ourselves.
    if lower_arg.startswith('/out:'):
      continue
    if (not lower_arg.startswith('/') and
        lower_arg.endswith(('.obj', '.lib', '.res'))):
      inputs.append(arg)
    else:
      flags.append(arg)

  return flags, inputs


def GetOriginalLinkerPath():
  try:
    val = _winreg.QueryValue(_winreg.HKEY_CURRENT_USER,
                             'Software\\Chromium\\split_link_installed')
    if os.path.exists(val):
      return val
  except WindowsError:
    pass

  raise SystemExit("Couldn't read linker location from registry")


def PartFor(input_file, description_parts, description_all):
  """Determines which part a given link input should be put into (or all)."""
  # Check if it should go in all parts.
  input_file = input_file.lower()
  if any(re.search(spec, input_file) for spec in description_all):
    return -1
  # Or pick which particular one it belongs in.
  for i, spec_list in enumerate(description_parts):
    if any(re.search(spec, input_file) for spec in spec_list):
      return i
  raise ValueError("couldn't find location for %s" % input_file)


def ParseOutExternals(output):
  """Given the stdout of link.exe, parses the error messages to retrieve all
  symbols that are unresolved."""
  result = set()
  # Styles of messages for unresolved externals, and a boolean to indicate
  # whether the error message emits the symbols with or without a leading
  # underscore.
  unresolved_regexes = [
    (re.compile(r' : error LNK2019: unresolved external symbol ".*" \((.*)\)'
                r' referenced in function'),
     False),
    (re.compile(r' : error LNK2001: unresolved external symbol ".*" \((.*)\)$'),
     False),
    (re.compile(r' : error LNK2019: unresolved external symbol (.*)'
                r' referenced in function '),
     True),
    (re.compile(r' : error LNK2001: unresolved external symbol (.*)$'),
     True),
  ]
  for line in output.splitlines():
    line = line.strip()
    for regex, strip_leading_underscore in unresolved_regexes:
      mo = regex.search(line)
      if mo:
        if strip_leading_underscore:
          result.add(mo.group(1)[1:])
        else:
          result.add(mo.group(1))
        break

  mo = re.search(r'fatal error LNK1120: (\d+) unresolved externals', output)
  # Make sure we have the same number that the linker thinks we have.
  assert mo or not result
  if len(result) != int(mo.group(1)):
    print output
    print 'Expecting %d, got %d' % (int(mo.group(1)), len(result))
  assert len(result) == int(mo.group(1))
  return sorted(result)


def AsCommandLineArgs(items):
  """Intended for output to a response file. Quotes all arguments."""
  return '\n'.join('"' + x + '"' for x in items)


def OutputNameForIndex(index):
  """Gets the final output DLL name, given a zero-based index."""
  if index == 0:
    return "chrome.dll"
  else:
    return 'chrome%d.dll' % index


def RunLinker(flags, index, inputs, phase):
  """Invokes the linker and returns the stdout, returncode and target name."""
  rspfile = 'part%d_%s.rsp' % (index, phase)
  with open(rspfile, 'w') as f:
    print >> f, AsCommandLineArgs(inputs)
    print >> f, AsCommandLineArgs(flags)
    output_name = OutputNameForIndex(index)
    print >> f, '/ENTRY:ChromeEmptyEntry@12'
    print >> f, '/OUT:' + output_name
  # Log('[[[\n' + open(rspfile).read() + '\n]]]')
  link_exe = GetOriginalLinkerPath()
  popen = subprocess.Popen([link_exe, '@' + rspfile], stdout=subprocess.PIPE)
  stdout, _ = popen.communicate()
  return stdout, popen.returncode, output_name


def GenerateDefFiles(unresolved_by_part):
  """Given a list of unresolved externals, generates a .def file that will
  cause all those symbols to be exported."""
  deffiles = []
  Log('generating .def files')
  for i, part in enumerate(unresolved_by_part):
    deffile = 'part%d.def' % i
    with open(deffile, 'w') as f:
      print >> f, 'LIBRARY %s' % OutputNameForIndex(i)
      print >> f, 'EXPORTS'
      for j, part in enumerate(unresolved_by_part):
        if i == j:
          continue
        print >> f, '\n'.join('  ' + export for export in part)
    deffiles.append(deffile)
  return deffiles


def BuildImportLibs(flags, inputs_by_part, deffiles):
  """Runs the linker to generate an import library."""
  import_libs = []
  Log('building import libs')
  for i, (inputs, deffile) in enumerate(zip(inputs_by_part, deffiles)):
    libfile = 'part%d.lib' % i
    flags_with_implib_and_deffile = flags + ['/IMPLIB:%s' % libfile,
                                             '/DEF:%s' % deffile]
    RunLinker(flags_with_implib_and_deffile, i, inputs, 'implib')
    import_libs.append(libfile)
  return import_libs


def AttemptLink(flags, inputs_by_part, unresolved_by_part, deffiles,
                import_libs):
  """Tries to run the linker for all parts using the current round of
  generated import libs and .def files. If the link fails, updates the
  unresolved externals list per part."""
  dlls = []
  all_succeeded = True
  new_externals = []
  Log('unresolveds now: %r' % [len(part) for part in unresolved_by_part])
  for i, (inputs, deffile) in enumerate(zip(inputs_by_part, deffiles)):
    Log('running link, part %d' % i)
    others_implibs = import_libs[:]
    others_implibs.pop(i)
    inputs_with_implib = inputs + filter(lambda x: x, others_implibs)
    if deffile:
      flags = flags + ['/DEF:%s' % deffile, '/LTCG']
    stdout, rc, output = RunLinker(flags, i, inputs_with_implib, 'final')
    if rc != 0:
      all_succeeded = False
      new_externals.append(ParseOutExternals(stdout))
    else:
      new_externals.append([])
      dlls.append(output)
  combined_externals = [sorted(set(prev) | set(new))
                        for prev, new in zip(unresolved_by_part, new_externals)]
  return all_succeeded, dlls, combined_externals


def main():
  flags, inputs = GetFlagsAndInputs(sys.argv[1:])
  partition_file = os.path.normpath(
      os.path.join(BASE_DIR, '../../../build/split_link_partition.py'))
  with open(partition_file) as partition:
    description = eval(partition.read())
  inputs_by_part = []
  description_parts = description['parts']
  # We currently assume that if a symbols isn't in dll 0, then it's in dll 1
  # when generating def files. Otherwise, we'd need to do more complex things
  # to figure out where each symbol actually is to assign it to the correct
  # .def file.
  num_parts = len(description_parts)
  assert num_parts == 2, "Can't handle > 2 dlls currently"
  description_parts.reverse()
  inputs_by_part = [[] for _ in range(num_parts)]
  for input_file in inputs:
    i = PartFor(input_file, description_parts, description['all'])
    if i == -1:
      for part in inputs_by_part:
        part.append(input_file)
    else:
      inputs_by_part[i].append(input_file)
  inputs_by_part.reverse()

  unresolved_by_part = [[] for _ in range(num_parts)]
  import_libs = [None] * num_parts
  deffiles = [None] * num_parts

  for i in range(5):
    Log('--- starting pass %d' % i)
    ok, dlls, unresolved_by_part = AttemptLink(
        flags, inputs_by_part, unresolved_by_part, deffiles, import_libs)
    if ok:
      break
    deffiles = GenerateDefFiles(unresolved_by_part)
    import_libs = BuildImportLibs(flags, inputs_by_part, deffiles)
  else:
    return 1
  Log('built %r' % dlls)

  return 0


if __name__ == '__main__':
  sys.exit(main())
