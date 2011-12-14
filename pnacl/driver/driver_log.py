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

import os
import sys
import pathtools

#TODO: DriverOpen/Close are in here because this is a low level lib without
# any dependencies. maybe they should go in another low level lib, or maybe
# this should become a low level lib that's more general than just log

# same with TempFiles. this factoring is at least a little useful though
# because it helps tease apart the dependencies. The TempName stuff is
# smarter and uses env, so it is not here for now.

def DriverOpen(filename, mode, fail_ok = False):
  try:
    fp = open(pathtools.tosys(filename), mode)
  except Exception:
    if not fail_ok:
      Log.Fatal("%s: Unable to open file", pathtools.touser(filename))
      DriverExit(1)
    else:
      return None
  return fp

def DriverClose(fp):
  fp.close()


class TempFileHandler(object):
  def __init__(self):
    self.files = []

  def add(self, path):
    path = pathtools.abspath(path)
    self.files.append(path)

  def wipe(self):
    for path in self.files:
      try:
        os.remove(path)
      except OSError:
        # If we're exiting early, the temp file
        # may have never been created.
        pass
    self.files = []

TempFiles = TempFileHandler()

def DriverExit(code):
  TempFiles.wipe()
  sys.exit(code)

######################################################################
#
# Logging
#
######################################################################

class Log(object):
  # Lists of streams
  prefix = ''
  LOG_OUT = []
  ERROR_OUT = [sys.stderr]

  @classmethod
  def reset(cls, log_to_file, log_filename, log_sizelimit):
    if log_to_file:
      cls.AddFile(log_filename, int(log_sizelimit))

  @classmethod
  def AddFile(cls, filename, sizelimit):
    file_too_big = pathtools.isfile(filename) and \
                   pathtools.getsize(filename) > sizelimit
    mode = 'a'
    if file_too_big:
      mode = 'w'
    fp = DriverOpen(filename, mode, fail_ok = True)
    if fp:
      cls.LOG_OUT.append(fp)

  @classmethod
  def Banner(cls, argv):
    cls.Info('-' * 60)
    cls.Info('PNaCl Driver Invoked With:\n' + StringifyCommand(argv))

  @classmethod
  def Info(cls, m, *args):
    cls.LogPrint(m, *args)

  @classmethod
  def Error(cls, m, *args):
    cls.ErrorPrint(m, *args)

  @classmethod
  def FatalWithResult(cls, ret, m, *args):
    m = 'FATAL: ' + m
    cls.LogPrint(m, *args)
    cls.ErrorPrint(m, *args)
    DriverExit(ret)

  @classmethod
  def Warning(cls, m, *args):
    m = 'Warning: ' + m
    cls.ErrorPrint(m, *args)

  @classmethod
  def Fatal(cls, m, *args):
    # Note, using keyword args and arg lists while trying to keep
    # the m and *args parameters next to each other does not work
    cls.FatalWithResult(-1, m, *args)

  @classmethod
  def LogPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.LOG_OUT:
      print >> o, m

  @classmethod
  def ErrorPrint(cls, m, *args):
    # NOTE: m may contain '%' if no args are given
    if args:
      m = m % args
    for o in cls.ERROR_OUT:
      print >> o, m

def EscapeEcho(s):
  """ Quick and dirty way of escaping characters that may otherwise be
      interpreted by bash / the echo command (rather than preserved). """
  return s.replace("\\", r"\\").replace("$", r"\$").replace('"', r"\"")

def StringifyCommand(cmd, stdin_contents=None):
  """ Return a string for reproducing the command "cmd", which will be
      fed stdin_contents through stdin. """
  stdin_str = ""
  if stdin_contents:
    stdin_str = "echo \"\"\"" + EscapeEcho(stdin_contents) + "\"\"\" | "
  return stdin_str + PrettyStringify(cmd)

def PrettyStringify(args):
  ret = ''
  grouping = 0
  for a in args:
    if grouping == 0 and len(ret) > 0:
      ret += " \\\n    "
    elif grouping > 0:
      ret += " "
    if grouping == 0:
      grouping = 1
      if a.startswith('-') and len(a) == 2:
        grouping = 2
    ret += a
    grouping -= 1
  return ret
