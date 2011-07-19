#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
IDLVersion for PPAPI

This file defines the behavior of the AST namespace which allows for resolving
a symbol as one or more AST nodes given a version or range of versions.
"""

import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions

Option('version_debug', 'Debug version data')
Option('wgap', 'Ignore version gap warning')


#
# Module level functions and data used for testing.
#
error = None
warning = None
def ReportVersionError(msg):
  global error
  error = msg

def ReportVersionWarning(msg):
  global warning
  warning = msg

def ReportClear():
  global error, warning
  error = None
  warning = None

#
# IDLVersion
#
# IDLVersion is an object which stores the association of a given symbol
# name, with an AST node for a range of versions for that object.
#
# A vmin value of None indicates that the object begins at the earliest
# available version number.  The value of vmin is always inclusive.

# A vmax value of None indicates that the object is never deprecated, so
# it exists until it is overloaded or until the latest available version.
# The value of vmax is always exclusive, representing the first version
# on which the object is no longer valid.
class IDLVersion(object):
  def __init__(self, vmin, vmax):
    self.vmin = vmin
    self.vmax = vmax

  def __str__(self):
    if not self.vmin:
      vmin = '0'
    else:
      vmin = str(self.vmin)
    if not self.vmax:
      vmax = '+oo'
    else:
      vmax = str(self.vmax)
    return '[%s,%s)' % (vmin, vmax)

  def SetVersionRange(self, vmin, vmax):
    if vmin is not None: vmin = float(vmin)
    if vmax is not None: vmax = float(vmax)
    self.vmin = vmin
    self.vmax = vmax

  # True, if version falls within the interval [self.vmin, self.vmax)
  def IsVersion(self, version):
    assert type(version) == float

    if self.vmax and self.vmax <= version:
      return False
    if self.vmin and self.vmin > version:
      return False
    if GetOption('version_debug'):
      InfoOut.Log('%f is in %s' % (version, self))
    return True

  # True, if interval [vmin, vmax) overlaps interval [self.vmin, self.vmax)
  def InRange(self, vmin, vmax):
    assert type(vmin) == float
    assert type(vmax) == float
    assert vmin != vmax

    if self.vmax and self.vmax <= vmin:
      return False
    if self.vmin and self.vmin >= vmax:
      return False

    if GetOption('version_debug'):
      InfoOut.Log('%f to %f is in %s' % (vmin, vmax, self))
    return True

  def Error(self, msg):
    ReportVersionError(msg)

  def Warning(self, msg):
    ReportVersionWarning(msg)


#
# Test Code
#
def Main(args):
  global errors

  FooXX = IDLVersion(None, None)
  Foo1X = IDLVersion(1.0, None)
  Foo23 = IDLVersion(2.0, 3.0)

  assert FooXX.IsVersion(0.0)
  assert FooXX.IsVersion(1.0)
  assert FooXX.InRange(0.0, 0.1)
  assert FooXX.InRange(1.0,2.0)

  assert not Foo1X.IsVersion(0.0)
  assert Foo1X.IsVersion(1.0)
  assert Foo1X.IsVersion(2.0)

  assert not Foo1X.InRange(0.0, 1.0)
  assert not Foo1X.InRange(0.5, 1.0)
  assert Foo1X.InRange(1.0, 2.0)
  assert Foo1X.InRange(2.0, 3.0)

  assert not Foo23.InRange(0.0, 1.0)
  assert not Foo23.InRange(0.5, 1.0)
  assert not Foo23.InRange(1.0, 2.0)
  assert Foo23.InRange(2.0, 3.0)
  assert Foo23.InRange(1.0, 2.1)
  assert Foo23.InRange(2.9, 4.0)
  assert not Foo23.InRange(3.0, 4.0)

  print "Passed"
  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

