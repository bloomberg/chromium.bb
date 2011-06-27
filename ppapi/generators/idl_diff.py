#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import subprocess
import sys

from idl_option import GetOption, Option, ParseOptions

#
# IDLDiff
#
# IDLDiff is a tool for comparing sets of IDL generated header files
# with the standard checked in headers.  It does this by capturing the
# output of the standard diff tool, parsing it into separate changes, then
# ignoring changes that are know to be safe, such as adding or removing
# blank lines, etc...
#

Option('gen', 'IDL generated files', default='hdir')
Option('src', 'Original ".h" files', default='../c')

# Change
#
# A Change object contains the previous lines, new news and change type.
#
class Change(object):
  def __init__(self, change):
    self.mode = str(change['mode'])
    self.was = list(change['was'])
    self.now = list(change['now'])

  def Dump(self):
    if not self.was:
      print 'Adding %s' % self.mode
    elif not self.now:
      print 'Missing %s' % self.mode
    else:
      print 'Modifying %s' % self.mode

    for line in self.was:
      print 'src: %s' % line
    for line in self.now:
      print 'gen: %s' % line
    print

#
# IsCopyright
#
# Return True if this change is only a one line change in the copyright notice
# such as non-matching years.
#
def IsCopyright(change):
  if len(change['now']) != 1 or len(change['was']) != 1: return False
  if 'Copyright (c)' not in change['now'][0]: return False
  if 'Copyright (c)' not in change['was'][0]: return False
  return True

#
# IsBlank
#
# Return True if this change only adds or removes blank lines
#
def IsBlank(change):
  for line in change['now']:
    if line: return False
  for line in change['was']:
    if line: return False
  return True


#
# IsSpacing
#
# Return True if this change is only different in the way 'words' are spaced
# such as in an enum:
#   ENUM_XXX   = 1,
#   ENUM_XYY_Y = 2,
# vs
#   ENUM_XXX = 1,
#   ENUM_XYY_Y = 2,
#
def IsSpacing(change):
  if len(change['now']) != len(change['was']): return False
  for i in range(len(change['now'])):
    # Also ignore right side comments
    line = change['was'][i]
    offs = line.find('//')
    if offs == -1:
      offs = line.find('/*')
    if offs >-1:
      line = line[:offs-1]

    words1 = change['now'][i].split()
    words2 = line.split()
    if words1 != words2: return False
  return True

#
# IsInclude
#
# Return True if change has extra includes
#
def IsInclude(change):
  if len(change['was']): return False
  for line in change['now']:
    if line and '#include' not in line: return False
  return True

#
# IsCppComment
#
# Return True if the change is only missing C++ comments
#
def IsCppComment(change):
  if len(change['now']): return False
  for line in change['was']:
    line = line.strip()
    if line[:2] != '//': return False
  return True
#
# ValidChange
#
# Return True if none of the changes does not patch an above "bogus" change.
#
def ValidChange(change):
  if IsCopyright(change): return False
  if IsBlank(change): return False
  if IsSpacing(change): return False
  if IsInclude(change): return False
  if IsCppComment(change): return False
  return True



#
# GetChanges
#
# Parse the output into discrete change blocks.
#
def GetChanges(output):
  lines = output.split('\n')
  changes = []
  change = { 'mode' : None,
             'was': [],
             'now': []
            }
  for line in lines:
    if not line: continue
    elif line[0] == '<':
      change['was'].append(line[2:])
    elif line[0] == '>':
      change['now'].append(line[2:])
    elif line[0] == '-':
      continue
    else:
      if ValidChange(change): changes.append(Change(change))
      change['mode'] = line
      change['now'] = []
      change['was'] = []
  if ValidChange(change): changes.append(Change(change))
  return changes

def Main(args):
  filenames = ParseOptions(args)
  if not filenames:
    gendir = os.path.join(GetOption('gen'), '*.h')
    filenames = glob.glob(gendir)

  for filename in filenames:
    gen = filename
    filename = filename[len(GetOption('gen')) + 1:]
    src = os.path.join(GetOption('src'), filename)
    p = subprocess.Popen(['diff', src, gen], stdout=subprocess.PIPE)
    output, errors = p.communicate()

    changes = GetChanges(output)
    if changes:
      print "\n\nDelta between:\n  src=%s\n  gen=%s\n" % (src, gen)
      for change in GetChanges(output):
        change.Dump()
    else:
      print "\nSAME:\n  src=%s\n  gen=%s\n" % (src, gen)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

