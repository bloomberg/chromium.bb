# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Error and information logging for IDL """

#
# IDL Log
#
# And IDLLog object provides a mechanism for capturing logging output
# and/or sending out via a file handle (usually stdout or stderr).
#
import sys


class IDLLog(object):
  def __init__(self, name, out):
    if name:
      self.name = '%s : ' % name
    else:
      self.name = ''

    self.out = out
    self.capture = False
    self.console = True
    self.log = []

  def Log(self, msg):
    line = "%s\n" % (msg)
    if self.console: self.out.write(line)
    if self.capture:
      self.log.append(msg)

  def LogTag(self, msg):
    line = "%s%s\n" % (self.name, msg)
    if self.console: self.out.write(line)
    if self.capture:
      self.log.append(msg)

  def LogLine(self, filename, lineno, pos, msg):
    line = "%s(%d) : %s%s\n" % (filename, lineno, self.name, msg)
    if self.console: self.out.write(line)
    if self.capture: self.log.append(msg)

  def SetConsole(self, enable, out = None):
    self.console = enable
    if out: self.out = out

  def SetCapture(self, enable):
    self.capture = enable

  def DrainLog(self):
    out = self.log
    self.log = []
    return out

ErrOut  = IDLLog('Error', sys.stderr)
WarnOut = IDLLog('Warning', sys.stdout)
InfoOut = IDLLog('', sys.stdout)
