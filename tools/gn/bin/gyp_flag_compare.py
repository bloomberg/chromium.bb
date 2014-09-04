#!/usr/bin/env python

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Given the output of -t commands from a ninja build for a gyp and GN generated
build, report on differences between the command lines."""


import sys


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


def GetFlags(lines):
  """Turn a list of command lines into a semi-structured dict."""
  flags_by_output = {}
  for line in lines:
    # TODO(scottmg): Hacky way of getting only cc for now.
    if 'clang' not in line:
      continue

    # TODO(scottmg): Proper escapes.
    command_line = line.strip().split()[1:]

    output_name = FindAndRemoveArgWithValue(command_line, '-o')
    dep_name = FindAndRemoveArgWithValue(command_line, '-MF')

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


def CompareLists(gyp, gn, name, dont_care=None):
  """Return a report of any differences between two lists, ignoring anything
  in |dont_care|."""
  if not dont_care:
    dont_care = []
  output = ''
  if gyp[name] != gn[name]:
    output += '  %s differ:\n' % name
    gyp_set = set(gyp[name]) - set(dont_care)
    gn_set = set(gn[name]) - set(dont_care)
    output += '    In gyp, but not in GN:\n      %s' % '\n      '.join(
        sorted(gyp_set - gn_set)) + '\n'
    output += '    In GN, but not in gyp:\n      %s' % '\n      '.join(
        sorted(gn_set - gyp_set)) + '\n\n'
  return output


def main():
  if len(sys.argv) != 3:
    print 'usage: %s gyp_commands.txt gn_commands.txt' % __file__
    return 1
  with open(sys.argv[1], 'rb') as f:
    gyp = f.readlines()
  with open(sys.argv[2], 'rb') as f:
    gn = f.readlines()
  all_gyp_flags = GetFlags(gyp)
  all_gn_flags = GetFlags(gn)
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
    differences += CompareLists(gyp_flags, gn_flags, 'defines', dont_care=[
        '-DENABLE_PRE_SYNC_BACKUP',
        '-DENABLE_WEBRTC=1',
        '-DUSE_LIBJPEG_TURBO=1',
        '-DUSE_PANGO=1',
        '-DUSE_SYMBOLIZE',
        ])
    differences += CompareLists(gyp_flags, gn_flags, 'include_dirs')
    differences += CompareLists(gyp_flags, gn_flags, 'warnings')
    differences += CompareLists(gyp_flags, gn_flags, 'other')
    if differences:
      files_with_given_differences.setdefault(differences, []).append(filename)

  for diff, files in files_with_given_differences.iteritems():
    print '\n'.join(sorted(files))
    print diff

  return 1 if files_with_given_differences or different_source_list else 0


if __name__ == '__main__':
  sys.exit(main())
