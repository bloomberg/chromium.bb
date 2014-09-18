#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given the output of -t commands from a ninja build for a gyp and GN generated
build, report on differences between the command lines."""


import os
import shlex
import subprocess
import sys


# Must be in src/.
os.chdir(os.path.join(os.path.dirname(__file__), '..', '..', '..'))


g_total_differences = 0


def FindAndRemoveArgWithValue(command_line, argname):
  """Given a command line as a list, remove and return the value of an option
  that takes a value as a separate entry.

  Modifies |command_line| in place.
  """
  if argname not in command_line:
    return ''
  location = command_line.index(argname)
  value = command_line[location + 1]
  command_line[location:location + 2] = []
  return value


def MergeSpacedArgs(command_line, argname):
  """Combine all arguments |argname| with their values, separated by a space."""
  i = 0
  result = []
  while i < len(command_line):
    arg = command_line[i]
    if arg == argname:
      result.append(arg + ' ' + command_line[i + 1])
      i += 1
    else:
      result.append(arg)
    i += 1
  return result


def NormalizeSymbolArguments(command_line):
  """Normalize -g arguments.

  If there's no -g args, it's equivalent to -g0. -g2 is equivalent to -g.
  Modifies |command_line| in place.
  """
  # Strip -g0 if there's no symbols.
  have_some_symbols = False
  for x in command_line:
    if x.startswith('-g') and x != '-g0':
      have_some_symbols = True
  if not have_some_symbols and '-g0' in command_line:
    command_line.remove('-g0')

  # Rename -g2 to -g.
  if '-g2' in command_line:
    command_line[index('-g2')] = '-g'


def GetFlags(lines):
  """Turn a list of command lines into a semi-structured dict."""
  flags_by_output = {}
  for line in lines:
    # TODO(scottmg): Hacky way of getting only cc for now.
    if 'clang' not in line:
      continue

    command_line = shlex.split(line.strip())[1:]

    output_name = FindAndRemoveArgWithValue(command_line, '-o')
    dep_name = FindAndRemoveArgWithValue(command_line, '-MF')

    NormalizeSymbolArguments(command_line)

    command_line = MergeSpacedArgs(command_line, '-Xclang')

    defines = [x for x in command_line if x.startswith('-D')]
    include_dirs = [x for x in command_line if x.startswith('-I')]
    dash_f = [x for x in command_line if x.startswith('-f')]
    warnings = [x for x in command_line if x.startswith('-W')]
    cc_file = [x for x in command_line if x.endswith('.cc') or
                                          x.endswith('.c') or
                                          x.endswith('.cpp')]
    if len(cc_file) != 1:
      print 'Skipping %s' % command_line
      continue
    assert len(cc_file) == 1
    others = [x for x in command_line if x not in defines and \
                                         x not in include_dirs and \
                                         x not in dash_f and \
                                         x not in warnings and \
                                         x not in cc_file]

    # Filter for libFindBadConstructs.so having a relative path in one and
    # absolute path in the other.
    others_filtered = []
    for x in others:
      if x.startswith('-Xclang ') and x.endswith('libFindBadConstructs.so'):
        others_filtered.append(
            '-Xclang ' +
            os.path.join(os.getcwd(),
                         os.path.normpath(
                             os.path.join('out/gn_flags', x.split(' ', 1)[1]))))
      elif x.startswith('-B'):
        others_filtered.append(
            '-B' +
            os.path.join(os.getcwd(),
                         os.path.normpath(os.path.join('out/gn_flags', x[2:]))))
      else:
        others_filtered.append(x)
    others = others_filtered

    flags_by_output[cc_file[0]] = {
      'output': output_name,
      'depname': dep_name,
      'defines': sorted(defines),
      'include_dirs': sorted(include_dirs),  # TODO(scottmg): This is wrong.
      'dash_f': sorted(dash_f),
      'warnings': sorted(warnings),
      'other': sorted(others),
    }
  return flags_by_output


def CompareLists(gyp, gn, name, dont_care_gyp=None, dont_care_gn=None):
  """Return a report of any differences between gyp and gn lists, ignoring
  anything in |dont_care_{gyp|gn}| respectively."""
  global g_total_differences
  if not dont_care_gyp:
    dont_care_gyp = []
  if not dont_care_gn:
    dont_care_gn = []
  output = ''
  if gyp[name] != gn[name]:
    gyp_set = set(gyp[name])
    gn_set = set(gn[name])
    missing_in_gyp = gyp_set - gn_set
    missing_in_gn = gn_set - gyp_set
    missing_in_gyp -= set(dont_care_gyp)
    missing_in_gn -= set(dont_care_gn)
    if missing_in_gyp or missing_in_gn:
      output += '  %s differ:\n' % name
    if missing_in_gyp:
      output += '    In gyp, but not in GN:\n      %s' % '\n      '.join(
          sorted(missing_in_gyp)) + '\n'
      g_total_differences += len(missing_in_gyp)
    if missing_in_gn:
      output += '    In GN, but not in gyp:\n      %s' % '\n      '.join(
          sorted(missing_in_gn)) + '\n\n'
      g_total_differences += len(missing_in_gn)
  return output


def Run(command_line):
  """Run |command_line| as a subprocess and return stdout. Raises on error."""
  return subprocess.check_output(command_line, shell=True)


def main():
  if len(sys.argv) != 2 and len(sys.argv) != 3:
    print 'usage: %s gyp_target gn_target' % __file__
    print '   or: %s target' % __file__
    return 1

  if len(sys.argv) == 2:
    sys.argv.append(sys.argv[1])

  print >>sys.stderr, 'Regenerating...'
  # Currently only Release, non-component.
  Run('gn gen out/gn_flags --args="is_debug=false is_component_build=false"')
  os.environ.pop('GYP_DEFINES', None)
  Run('python build/gyp_chromium -Goutput_dir=out_gyp_flags -Gconfig=Release')
  gn = Run('ninja -C out/gn_flags -t commands %s' % sys.argv[2])
  gyp = Run('ninja -C out_gyp_flags/Release -t commands %s' % sys.argv[1])
  all_gyp_flags = GetFlags(gyp.splitlines())
  all_gn_flags = GetFlags(gn.splitlines())
  gyp_files = set(all_gyp_flags.keys())
  gn_files = set(all_gn_flags.keys())
  different_source_list = gyp_files != gn_files
  if different_source_list:
    print 'Different set of sources files:'
    print '  In gyp, not in GN:\n    %s' % '\n    '.join(
        sorted(gyp_files - gn_files))
    print '  In GN, not in gyp:\n    %s' % '\n    '.join(
        sorted(gn_files - gyp_files))
    print '\nNote that flags will only be compared for files in both sets.\n'
  file_list = gyp_files & gn_files
  files_with_given_differences = {}
  for filename in sorted(file_list):
    gyp_flags = all_gyp_flags[filename]
    gn_flags = all_gn_flags[filename]
    differences = CompareLists(gyp_flags, gn_flags, 'dash_f')
    differences += CompareLists(gyp_flags, gn_flags, 'defines')
    differences += CompareLists(gyp_flags, gn_flags, 'include_dirs')
    differences += CompareLists(gyp_flags, gn_flags, 'warnings', dont_care_gn=[
        # More conservative warnings in GN we consider to be OK.
        '-Wendif-labels',
        '-Wextra',
        '-Wsign-compare',
        ])
    differences += CompareLists(gyp_flags, gn_flags, 'other')
    if differences:
      files_with_given_differences.setdefault(differences, []).append(filename)

  for diff, files in files_with_given_differences.iteritems():
    print '\n'.join(sorted(files))
    print diff

  print 'Total differences:', g_total_differences
  # TODO(scottmg): Return failure on difference once we're closer to identical.
  return 0


if __name__ == '__main__':
  sys.exit(main())
