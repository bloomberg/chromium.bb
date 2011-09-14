#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Output file objects for generator. """

import difflib
import os
import time
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions
from stat import *

Option('diff', 'Generate a DIFF when saving the file.')

def IsEquivelent(intext, outtext):
  if not intext: return False
  inlines = intext.split('\n')
  outlines = outtext.split('\n')

  # If number of lines don't match, it's a mismatch
  if len(inlines) != len(outlines):  return False

  for index in range(len(inlines)):
    inline = inlines[index]
    outline = outlines[index]

    if inline == outline: continue

    # If the line is not an exact match, check for comment deltas
    inwords = inline.split()
    outwords = outline.split()

    if not inwords or not outwords: return False
    if inwords[0] != outwords[0] or inwords[0] != '/*': return False

    # Neither the year, nor the modified date need an exact match
    if inwords[1] == 'Copyright':
      if inwords[4:] == outwords[4:]: continue
    elif inwords[1] == 'From':
      if inwords[0:4] == outwords[0:4]:
        continue
    return False
  return True


#
# IDLOutFile
#
# IDLOutFile provides a temporary output file.  By default, the object will
# not write the output if the file already exists, and matches what will be
# written.  This prevents the timestamp from changing to optimize cases where
# the output files are used by a timestamp dependent build system
#
class IDLOutFile(object):
  def __init__(self, filename, always_write = False, create_dir = True):
    self.filename = filename
    self.always_write = always_write
    self.create_dir = create_dir
    self.outlist = []
    self.open = True

  # Return the file name
  def Filename(self):
    return self.filename

  # Append to the output if the file is still open
  def Write(self, string):
    if not self.open:
      raise RuntimeError('Could not write to closed file %s.' % self.filename)
    self.outlist.append(string)

  # Close the file
  def Close(self):
    filename = os.path.realpath(self.filename)
    self.open = False
    outtext = ''.join(self.outlist)
    if not self.always_write:
      if os.path.isfile(filename):
        intext = open(filename, 'r').read()
      else:
        intext = ''

      if IsEquivelent(intext, outtext):
        if GetOption('verbose'):
          InfoOut.Log('Output %s unchanged.' % self.filename)
        return False

    if GetOption('diff'):
      for line in difflib.unified_diff(intext.split('\n'), outtext.split('\n'),
                                       'OLD ' + self.filename,
                                       'NEW ' + self.filename,
                                       n=1, lineterm=''):
        ErrOut.Log(line)

    try:
      # If the directory does not exit, try to create it, if we fail, we
      # still get the exception when the file is openned.
      basepath, leafname = os.path.split(filename)
      if basepath and not os.path.isdir(basepath) and self.create_dir:
        InfoOut.Log('Creating directory: %s\n' % basepath)
        os.makedirs(basepath)

      if not GetOption('test'):
        outfile = open(filename, 'w')
        outfile.write(outtext)
        InfoOut.Log('Output %s written.' % self.filename)
      return True

    except IOError as (errno, strerror):
      ErrOut.Log("I/O error(%d): %s" % (errno, strerror))
    except:
      ErrOut.Log("Unexpected error: %s" % sys.exc_info()[0])
      raise

    return False

def TestFile(name, stringlist, force, update):
  errors = 0

  # Get the old timestamp
  if os.path.exists(name):
    old_time = os.stat(filename)[ST_MTIME]
  else:
    old_time = 'NONE'

  # Create the file and write to it
  out = IDLOutFile(filename, force)
  for item in stringlist:
    out.Write(item)

  # We wait for flush to force the timestamp to change
  time.sleep(2)

  wrote = out.Close()
  cur_time = os.stat(filename)[ST_MTIME]
  if update:
    if not wrote:
      ErrOut.Log('Failed to write output %s.' % filename)
      return 1
    if cur_time == old_time:
      ErrOut.Log('Failed to update timestamp for %s.' % filename)
      return 1
  else:
    if wrote:
      ErrOut.Log('Should not have writen output %s.' % filename)
      return 1
    if cur_time != old_time:
      ErrOut.Log('Should not have modified timestamp for %s.' % filename)
      return 1
  return 0

if __name__ == '__main__':

  errors = 0
  stringlist = ['Test', 'Testing\n', 'Test']
  filename = 'outtest.txt'

  # Test forcibly writing a file
  errors += TestFile(filename, stringlist, force=True, update=True)

  # Test conditionally writing the file skipping
  errors += TestFile(filename, stringlist, force=False, update=False)

  # Test conditionally writing the file updating
  errors += TestFile(filename, stringlist + ['X'], force=False, update=True)

  # Clean up file
  os.remove(filename)
  if not errors: InfoOut.Log('All tests pass.')
  sys.exit(errors)

