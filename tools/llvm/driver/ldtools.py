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
  fp = driver_tools.DriverOpen(filename, 'r')

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
