#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#

# Tool for reading linker script

# TODO(pdox): Refactor driver_tools so that there is no circular dependency.
import driver_tools
import os
import pathtools
import driver_log

def IsLinkerScript(filename):
  return ParseLinkerScript(filename) is not None

# Parses a linker script to determine additional ld arguments specified.
# Returns a list of linker arguments.
#
# For example, if the linker script contains
#
#     GROUP ( libc.so.6 libc_nonshared.a  AS_NEEDED ( ld-linux.so.2 ) )
#
# Then this function will return:
#
#     ['--start-group', '-l:libc.so.6', '-l:libc_nonshared.a',
#      '--as-needed', '-l:ld-linux.so.2', '--no-as-needed', '--end-group']
#
# Returns None on any parse error.
def ParseLinkerScript(filename):
  fp = driver_log.DriverOpen(filename, 'r')

  ret = []
  stack = []
  expect = ''  # Expected next token
  while True:
    token = GetNextToken(fp)
    if token is None:
      # Tokenization error
      return None

    if not token:
      # EOF
      break

    if expect:
      if token == expect:
        expect = ''
        continue
      else:
        return None

    if not stack:
      if token == 'INPUT':
        expect = '('
        stack.append(token)
      elif token == 'GROUP':
        expect = '('
        ret.append('--start-group')
        stack.append(token)
      elif token == 'OUTPUT_FORMAT':
        expect = '('
        stack.append(token)
      elif token == 'EXTERN':
        expect = '('
        stack.append(token)
      elif token == ';':
        pass
      else:
        return None
    else:
      if token == ')':
        section = stack.pop()
        if section == 'AS_NEEDED':
          ret.append('--no-as-needed')
        elif section == 'GROUP':
          ret.append('--end-group')
      elif token == 'AS_NEEDED':
        expect = '('
        ret.append('--as-needed')
        stack.append('AS_NEEDED')
      elif stack[-1] == 'OUTPUT_FORMAT':
        # Ignore stuff inside OUTPUT_FORMAT
        pass
      elif stack[-1] == 'EXTERN':
        ret.append('--undefined=' + token)
      else:
        ret.append('-l:' + token)

  fp.close()
  return ret


# Get the next token from the linker script
# Returns: ''   for EOF.
#          None on error.
def GetNextToken(fp):
  token = ''
  while True:
    ch = fp.read(1)

    if not ch:
      break

    # Whitespace terminates a token
    # (but ignore whitespace before the token)
    if ch in (' ', '\t', '\n'):
      if token:
        break
      else:
        continue

    # ( and ) are tokens themselves (or terminate existing tokens)
    if ch in ('(',')'):
      if token:
        fp.seek(-1, os.SEEK_CUR)
        break
      else:
        token = ch
        break

    token += ch
    if token.endswith('/*'):
      if not ReadPastComment(fp, '*/'):
        return None
      token = token[:-2]

  return token

def ReadPastComment(fp, terminator):
  s = ''
  while True:
    ch = fp.read(1)
    if not ch:
      return False
    s += ch
    if s.endswith(terminator):
      break

  return True

##############################################################################

def FindFirstLinkerScriptInput(inputs):
  for i in xrange(len(inputs)):
    f = inputs[i]
    if IsFlag(f):
      continue
    if driver_tools.FileType(f) == 'ldscript':
      return (i, f)
  return (None, None)

def ExpandLinkerScripts(inputs, searchdirs, static_only):
  while True:
    # Find a ldscript in the input list
    i, path = FindFirstLinkerScriptInput(inputs)

    # If none, we are done.
    if path is None:
      break

    new_inputs = ParseLinkerScript(path)
    ExpandLibFlags(new_inputs, searchdirs, static_only)
    inputs = inputs[:i] + new_inputs + inputs[i+1:]
  return inputs

def ExpandLibFlags(inputs, searchdirs, static_only):
  """ Given an input list, expand -lfoo or -l:foo.so
      into a full filename. Returns the new input list """
  for i in xrange(len(inputs)):
    f = inputs[i]
    if IsFlag(f):
      continue
    if IsLib(f):
      inputs[i] = FindLib(f, searchdirs, static_only)

def IsFlag(arg):
  return arg.startswith('-') and not IsLib(arg)

def IsLib(arg):
  return arg.startswith('-l')

def FindLib(arg, searchdirs, static_only):
  """Returns the full pathname for the library input.
     For example, name might be "-lc" or "-lm".
     Returns None if the library is not found.
  """
  assert(IsLib(arg))
  assert(searchdirs is not None)
  name = arg[len('-l'):]
  if not name:
    driver_log.Log.Fatal("-l missing library name")
  is_whole_name = (name[0] == ':')

  searchnames = []
  if is_whole_name:
    # -l:filename  (search for the filename)
    name = name[1:]
    searchnames.append(name)

    # .pso may exist in lieu of .so, or vice versa.
    if '.so' in name:
      searchnames.append(name.replace('.so', '.pso'))
    if '.pso' in name:
      searchnames.append(name.replace('.pso', '.so'))
  else:
    # -lfoo
    if static_only:
      extensions = [ 'a' ]
    else:
      extensions = [ 'pso', 'so', 'a' ]
    for ext in extensions:
      searchnames.append('lib' + name + '.' + ext)

  foundpath = FindFile(searchnames, searchdirs)
  if foundpath:
    return foundpath

  if is_whole_name:
    label = name
  else:
    label = arg
  driver_log.Log.Fatal("Cannot find '%s'", label)

def FindFile(search_names, search_dirs):
  for curdir in search_dirs:
    for name in search_names:
      path = pathtools.join(curdir, name)
      if pathtools.exists(path):
        return path
  return None

def ExpandInputs(inputs, searchdirs, static_only):
  # Expand all -l flags into filenames
  ExpandLibFlags(inputs, searchdirs, static_only)

  # Expand input files which are linker scripts
  inputs = ExpandLinkerScripts(inputs, searchdirs, static_only)
  return inputs
