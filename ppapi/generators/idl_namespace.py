#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
IDLNamespace for PPAPI

This file defines the behavior of the AST namespace which allows for resolving
a symbol as one or more AST nodes given a version or range of versions.
"""

import sys

from idl_option import GetOption, Option, ParseOptions
from idl_log import ErrOut, InfoOut, WarnOut
from idl_version import IDLVersion

Option('label', 'Use the specifed label blocks.', default='Chrome')
Option('namespace_debug', 'Use the specified version')


#
# IDLVersionList
#
# IDLVersionList is a list based container for holding IDLVersion
# objects in order.  The IDLVersionList can be added to, and searched by
# range.  Objects are stored in order, and must be added in order.
#
class IDLVersionList(object):
  def __init__(self):
    self.nodes = []

  def FindVersion(self, version):
    assert type(version) == float

    for node in self.nodes:
      if node.IsVersion(version):
        return node
    return None

  def FindRange(self, vmin, vmax):
    assert type(vmin) == float
    assert type(vmax) == float
    assert vmin != vmax

    out = []
    for node in self.nodes:
      if node.InRange(vmin, vmax):
        out.append(node)
    return out

  def AddNode(self, node):
    if GetOption('version_debug'):
      InfoOut.Log('\nAdding %s %s' % (node.Location(), node))
    last = None

    # Check current versions in that namespace
    for cver in self.nodes:
      if GetOption('version_debug'): InfoOut.Log('  Checking %s' % cver)

      # We should only be missing a 'version' tag for the first item.
      if not node.vmin:
        raise RuntimeError('Missing version')
        node.Error('Missing version on overload of previous %s.' %
                   cver.Location())
        return False

      # If the node has no max, then set it to this one
      if not cver.vmax:
        cver.vmax = node.vmin
        if GetOption('version_debug'): InfoOut.Log('  Update %s' % cver)

      # if the max and min overlap, than's an error
      if cver.vmax > node.vmin:
        if node.vmax and cver.vmin >= node.vmax:
          node.Error('Declarations out of order.')
        else:
          node.Error('Overlap in versions.')
        return False
      last = cver

    # Otherwise, the previous max and current min should match
    # unless this is the unlikely case of something being only
    # temporarily deprecated.
    if last and last.vmax != node.vmin:
      node.Warn('Gap in version numbers.')

    # If we made it here, this new node must be the 'newest'
    # and does not overlap with anything previously added, so
    # we can add it to the end of the list.
    if GetOption('version_debug'): InfoOut.Log('Done %s' % node)
    self.nodes.append(node)
    return True

#
# IDLVersionMap
#
# A version map, can map from an float interface version, to a global
# release string.
#
class IDLVersionMap(object):
  def __init__(self):
    self.version_to_release = {}
    self.release_to_version = {}
    self.versions = []
    self.releases = []

  def AddReleaseVersionMapping(self, release, version):
    self.version_to_release[version] = release
    self.release_to_version[release] = version
    self.versions = sorted(self.version_to_release.keys())
    self.releases = sorted(self.release_to_version.keys())

  def GetRelease(self, version):
    # Check for exact match
    if version in self.versions:
      return self.version_to_release[version]

  def GetVersion(self, release):
    if release > self.releases[-1]:
      release = self.releases[-1]
    elif release < self.releases[0]:
      release = self.releases[0]
    return self.release_to_version[release]



#
# IDLNamespace
#
# IDLNamespace provides a mapping between a symbol name and an IDLVersionList
# which contains IDLVersion objects.  It provides an interface for fetching
# one or more IDLNodes based on a version or range of versions.
#
class IDLNamespace(object):
  def __init__(self, parent):
    self.namespace = {}
    self.parent = parent

  def Dump(self):
    for name in self.namespace:
      InfoOut.Log('NAME=%s' % name)
      for cver in self.namespace[name]:
        InfoOut.Log('  %s' % cver)
      InfoOut.Log('')

  def FindVersion(self, name, version):
    verlist = self.namespace.get(name, None)
    if verlist == None:
      if self.parent:
        return self.parent.FindVersion(name, version)
      else:
        return None
    return verlist.FindVersion(version)

  def FindRange(self, name, vmin, vmax):
    verlist = self.namespace.get(name, None)
    if verlist == None:
      if self.parent:
        return self.parent.FindRange(name, vmin, vmax)
      else:
        return []
    return verlist.FindRange(vmin, vmax)

  def FindList(self, name):
    verlist = self.namespace.get(name, None)
    if verlist == None:
      if self.parent:
        return self.parent.FindList(name)
    return verlist

  def AddNode(self, node):
    name = node.GetName()
    verlist = self.namespace.setdefault(name,IDLVersionList())
    if GetOption('namespace_debug'):
        print "Adding to namespace: %s" % node
    return verlist.AddNode(node)



#
# Testing Code
#

#
# MockNode
#
# Mocks the IDLNode to support error, warning handling, and string functions.
#
class MockNode(IDLVersion):
  def __init__(self, name, vmin, vmax):
    self.name = name
    self.vmin = vmin
    self.vmax = vmax
    self.errors = []
    self.warns = []
    self.properties = {
        'NAME': name,
        'version': vmin,
        'deprecate' : vmax
        }

  def __str__(self):
    return '%s (%s : %s)' % (self.name, self.vmin, self.vmax)

  def GetName(self):
    return self.name

  def Error(self, msg):
    if GetOption('version_debug'): print 'Error: %s' % msg
    self.errors.append(msg)

  def Warn(self, msg):
    if GetOption('version_debug'): print 'Warn: %s' % msg
    self.warns.append(msg)

  def GetProperty(self, name):
    return self.properties.get(name, None)

errors = 0
#
# DumpFailure
#
# Dumps all the information relevant  to an add failure.
def DumpFailure(namespace, node, msg):
    global errors
    print '\n******************************'
    print 'Failure: %s %s' % (node, msg)
    for warn in node.warns:
      print '  WARN: %s' % warn
    for err in node.errors:
      print '  ERROR: %s' % err
    print '\n'
    namespace.Dump()
    print '******************************\n'
    errors += 1

# Add expecting no errors or warnings
def AddOkay(namespace, node):
  okay = namespace.AddNode(node)
  if not okay or node.errors or node.warns:
    DumpFailure(namespace, node, 'Expected success')

# Add expecting a specific warning
def AddWarn(namespace, node, msg):
  okay = namespace.AddNode(node)
  if not okay or node.errors or not node.warns:
    DumpFailure(namespace, node, 'Expected warnings')
  if msg not in node.warns:
    DumpFailure(namespace, node, 'Expected warning: %s' % msg)

# Add expecting a specific error any any number of warnings
def AddError(namespace, node, msg):
  okay = namespace.AddNode(node)
  if okay or not node.errors:
    DumpFailure(namespace, node, 'Expected errors')
  if msg not in node.errors:
    DumpFailure(namespace, node, 'Expected error: %s' % msg)

# Verify that a FindVersion call on the namespace returns the expected node.
def VerifyFindOne(namespace, name, version, node):
  global errors
  if (namespace.FindVersion(name, version) != node):
    print "Failed to find %s as version %f of %s" % (node, version, name)
    namespace.Dump()
    print "\n"
    errors += 1

# Verify that a FindRage call on the namespace returns a set of expected nodes.
def VerifyFindAll(namespace, name, vmin, vmax, nodes):
  global errors
  out = namespace.FindRange(name, vmin, vmax)
  if (out != nodes):
    print "Found [%s] instead of[%s] for versions %f to %f of %s" % (
        ' '.join([str(x) for x in out]),
        ' '.join([str(x) for x in nodes]),
        vmin,
        vmax,
        name)
    namespace.Dump()
    print "\n"
    errors += 1

def Main(args):
  global errors
  ParseOptions(args)

  InfoOut.SetConsole(True)

  namespace = IDLNamespace(None)

  FooXX = MockNode('foo', None, None)
  Foo1X = MockNode('foo', 1.0, None)
  Foo2X = MockNode('foo', 2.0, None)
  Foo3X = MockNode('foo', 3.0, None)

  # Verify we succeed with undeprecated adds
  AddOkay(namespace, FooXX)
  AddOkay(namespace, Foo1X)
  AddOkay(namespace, Foo3X)
  # Verify we fail to add a node between undeprecated versions
  AddError(namespace, Foo2X, 'Overlap in versions.')

  BarXX = MockNode('bar', None, None)
  Bar12 = MockNode('bar', 1.0, 2.0)
  Bar23 = MockNode('bar', 2.0, 3.0)
  Bar34 = MockNode('bar', 3.0, 4.0)


  # Verify we succeed with fully qualified versions
  namespace = IDLNamespace(namespace)
  AddOkay(namespace, BarXX)
  AddOkay(namespace, Bar12)
  # Verify we warn when detecting a gap
  AddWarn(namespace, Bar34, 'Gap in version numbers.')
  # Verify we fail when inserting into this gap
  # (NOTE: while this could be legal, it is sloppy so we disallow it)
  AddError(namespace, Bar23, 'Declarations out of order.')

  # Verify local namespace
  VerifyFindOne(namespace, 'bar', 0.0, BarXX)
  VerifyFindAll(namespace, 'bar', 0.5, 1.5, [BarXX, Bar12])

  # Verify the correct version of the object is found recursively
  VerifyFindOne(namespace, 'foo', 0.0, FooXX)
  VerifyFindOne(namespace, 'foo', 0.5, FooXX)
  VerifyFindOne(namespace, 'foo', 1.0, Foo1X)
  VerifyFindOne(namespace, 'foo', 1.5, Foo1X)
  VerifyFindOne(namespace, 'foo', 3.0, Foo3X)
  VerifyFindOne(namespace, 'foo', 100.0, Foo3X)

  # Verify the correct range of objects is found
  VerifyFindAll(namespace, 'foo', 0.0, 1.0, [FooXX])
  VerifyFindAll(namespace, 'foo', 0.5, 1.0, [FooXX])
  VerifyFindAll(namespace, 'foo', 1.0, 1.1, [Foo1X])
  VerifyFindAll(namespace, 'foo', 0.5, 1.5, [FooXX, Foo1X])
  VerifyFindAll(namespace, 'foo', 0.0, 3.0, [FooXX, Foo1X])
  VerifyFindAll(namespace, 'foo', 3.0, 100.0, [Foo3X])

  FooBar = MockNode('foobar', 1.0, 2.0)
  namespace = IDLNamespace(namespace)
  AddOkay(namespace, FooBar)

  if errors:
    print 'Test failed with %d errors.' % errors
  else:
    print 'Passed.'
  return errors

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

