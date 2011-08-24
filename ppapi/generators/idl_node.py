#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Nodes for PPAPI IDL AST"""

#
# IDL Node
#
# IDL Node defines the IDLAttribute and IDLNode objects which are constructed
# by the parser as it processes the various 'productions'.  The IDLAttribute
# objects are assigned to the IDLNode's property dictionary instead of being
# applied as children of The IDLNodes, so they do not exist in the final tree.
# The AST of IDLNodes is the output from the parsing state and will be used
# as the source data by the various generators.
#

import hashlib
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_propertynode import IDLPropertyNode
from idl_namespace import IDLNamespace
from idl_version import IDLVersion


# IDLAttribute
#
# A temporary object used by the parsing process to hold an Extended Attribute
# which will be passed as a child to a standard IDLNode.
#
class IDLAttribute(object):
  def __init__(self, name, value):
    self.cls = 'ExtAttribute'
    self.name = name
    self.value = value

  def __str__(self):
    return '%s=%s' % (self.name, self.value)


#
# IDLNode
#
# This class implements the AST tree, providing the associations between
# parents and children.  It also contains a namepsace and propertynode to
# allow for look-ups.  IDLNode is derived from IDLVersion, so it is
# version aware.
#
class IDLNode(IDLVersion):

  # Set of object IDLNode types which have a name and belong in the namespace.
  NamedSet = set(['Enum', 'EnumItem', 'File', 'Function', 'Interface',
                  'Member', 'Param', 'Struct', 'Type', 'Typedef'])

  show_versions = False
  def __init__(self, cls, filename, lineno, pos, children=None):
    # Initialize with no starting or ending Version
    IDLVersion.__init__(self, None, None)

    self.cls = cls
    self.lineno = lineno
    self.pos = pos
    self.filename = filename
    self.hashes = {}
    self.deps = {}
    self.errors = 0
    self.namespace = None
    self.typelist = None
    self.parent = None
    self.property_node = IDLPropertyNode()
    # self.children is a list of children ordered as defined
    self.children = []
    # Process the passed in list of children, placing ExtAttributes into the
    # property dictionary, and nodes into the local child list in order.  In
    # addition, add nodes to the namespace if the class is in the NamedSet.
    if not children: children = []
    for child in children:
      if child.cls == 'ExtAttribute':
        self.SetProperty(child.name, child.value)
      else:
        self.AddChild(child)

#
# String related functions
#
#

  # Return a string representation of this node
  def __str__(self):
    name = self.GetName()
    ver = IDLVersion.__str__(self)
    if name is None: name = ''
    if not IDLNode.show_versions: ver = ''
    return '%s(%s%s)' % (self.cls, name, ver)

  # Return file and line number for where node was defined
  def Location(self):
    return '%s(%d)' % (self.filename, self.lineno)

  # Log an error for this object
  def Error(self, msg):
    self.errors += 1
    ErrOut.LogLine(self.filename, self.lineno, 0, ' %s %s' %
                   (str(self), msg))
    if self.lineno == 46: raise Exception("huh?")

  # Log a warning for this object
  def Warning(self, msg):
    WarnOut.LogLine(self.filename, self.lineno, 0, ' %s %s' %
                    (str(self), msg))

  def GetName(self):
    return self.GetProperty('NAME')

  def GetNameVersion(self):
    name = self.GetProperty('NAME', default='')
    ver = IDLVersion.__str__(self)
    return '%s%s' % (name, ver)

  # Dump this object and its children
  def Dump(self, depth=0, comments=False, out=sys.stdout):
    if self.cls in ['Comment', 'Copyright']:
      is_comment = True
    else:
      is_comment = False

    # Skip this node if it's a comment, and we are not printing comments
    if not comments and is_comment: return

    tab = ''.rjust(depth * 2)

    if is_comment:
      for line in self.GetName().split('\n'):
        out.write('%s%s\n' % (tab, line))
    else:
      out.write('%s%s\n' % (tab, self))
    properties = self.property_node.GetProperyList()
    if properties:
      out.write('%s  Properties\n' % tab)
      for p in properties:
        out.write('%s    %s : %s\n' % (tab, p, self.GetProperty(p)))
    for child in self.children:
      child.Dump(depth+1, comments=comments, out=out)

#
# Search related functions
#

  # Check if node is of a given type
  def IsA(self, *typelist):
    if self.cls in typelist: return True
    return False

  # Get a list of objects for this key
  def GetListOf(self, *keys):
    out = []
    for child in self.children:
      if child.cls in keys: out.append(child)
    return out

  def GetOneOf(self, *keys):
    out = self.GetListOf(*keys)
    if out: return out[0]
    return None

  def SetParent(self, parent):
    self.property_node.AddParent(parent)
    self.parent = parent

  def AddChild(self, node):
    node.SetParent(self)
    self.children.append(node)

  # Get a list of all children
  def GetChildren(self):
    return self.children

  # Get a list of all children of a given version
  def GetChildrenVersion(self, version):
    out = []
    for child in self.children:
      if child.IsVersion(version): out.append(child)
    return out

  # Get a list of all children in a given range
  def GetChildrenRange(self, vmin, vmax):
    out = []
    for child in self.children:
      if child.IsRange(vmin, vmax): out.append(child)
    return out

  def FindVersion(self, name, version):
    node = self.namespace.FindNode(name, version)
    if not node and self.parent:
      node = self.parent.FindVersion(name, version)
    return node

  def FindRange(self, name, vmin, vmax):
    nodes = self.namespace.FindNodes(name, vmin, vmax)
    if not nodes and self.parent:
      nodes = self.parent.FindVersion(name, vmin, vmax)
    return nodes

  def IsRelease(self, release):
    label = self.GetLabel()
    # Assume object is always available if there is no Label
    if not label:
      return True

    version = label.GetVersion(release)
    out =  self.IsVersion(version)
    return out

  def InReleases(self, releases):
    for rel in releases:
      if self.IsRelease(rel): return True
    return False

  def GetLabel(self):
    label = self.GetProperty('LABEL')
    if not label:
      self.Error('No label availible.')
      return None
    return label

  def GetType(self, release):
    label = self.GetLabel()
    if not label: return None
    if not self.typelist: return None
    version = label.GetVersion(release)
    return self.typelist.FindVersion(version)

  def GetHash(self, release):
    hashval = self.hashes.get(release, None)
    if hashval is None:
      hashval = hashlib.sha1()
      hashval.update(self.cls)
      for key in self.property_node.GetPropertyList():
        val = self.GetProperty(key)
        hashval.update('%s=%s' % (key, str(val)))
      typeref = self.GetType(release)
      if typeref:
        hashval.update(typeref.GetHash(release))
      for child in self.GetChildren():
        if child.IsA('Copyright', 'Comment', 'Label'): continue
        if not child.IsRelease(release):
          continue
        hashval.update( child.GetHash(release) )
      self.hashes[release] = hashval
    return hashval.hexdigest()

  def GetDeps(self, release):
    deps = self.deps.get(release, None)
    if deps is None:
      deps = set([self])
      for child in self.GetChildren():
        deps |= child.GetDeps(release)
      typeref = self.GetType(release)
      if typeref: deps |= typeref.GetDeps(release)
      self.deps[release] = deps
    return deps

  def GetRelease(self, version):
    label = self.GetLabel()
    if not label: return None
    return label.GetRelease(version)

  def GetVersion(self, release):
    label = self.GetLabel()
    if not label: return None
    return label.GetVersion(release)

  def SetProperty(self, name, val):
    self.property_node.SetProperty(name, val)

  def GetProperty(self, name, default=None):
    return self.property_node.GetProperty(name, default)

  def Traverse(self, data, func):
    func(self, data)
    for child in self.children:
      child.Traverse(data, func)


#
# IDLFile
#
# A specialized version of IDLNode which tracks errors and warnings.
#
class IDLFile(IDLNode):
  def __init__(self, name, children, errors=0):
    attrs = [IDLAttribute('NAME', name),
             IDLAttribute('ERRORS', errors)]
    if not children: children = []
    IDLNode.__init__(self, 'File', name, 1, 0, attrs + children)


#
# Tests
#
def StringTest():
  errors = 0
  name_str = 'MyName'
  text_str = 'MyNode(%s)' % name_str
  name_node = IDLAttribute('NAME', name_str)
  node = IDLNode('MyNode', 'no file', 1, 0, [name_node])
  if node.GetName() != name_str:
    ErrOut.Log('GetName returned >%s< not >%s<' % (node.GetName(), name_str))
    errors += 1
  if node.GetProperty('NAME') != name_str:
    ErrOut.Log('Failed to get name property.')
    errors += 1
  if str(node) != text_str:
    ErrOut.Log('str() returned >%s< not >%s<' % (str(node), text_str))
    errors += 1
  if not errors: InfoOut.Log('Passed StringTest')
  return errors


def ChildTest():
  errors = 0
  child = IDLNode('child', 'no file', 1, 0)
  parent = IDLNode('parent', 'no file', 1, 0, [child])

  if child.parent != parent:
    ErrOut.Log('Failed to connect parent.')
    errors += 1

  if [child] != parent.GetChildren():
    ErrOut.Log('Failed GetChildren.')
    errors += 1

  if child != parent.GetOneOf('child'):
    ErrOut.Log('Failed GetOneOf(child)')
    errors += 1

  if parent.GetOneOf('bogus'):
    ErrOut.Log('Failed GetOneOf(bogus)')
    errors += 1

  if not parent.IsA('parent'):
    ErrOut.Log('Expecting parent type')
    errors += 1

  parent = IDLNode('parent', 'no file', 1, 0, [child, child])
  if [child, child] != parent.GetChildren():
    ErrOut.Log('Failed GetChildren2.')
    errors += 1

  if not errors: InfoOut.Log('Passed ChildTest')
  return errors


def Main():
  errors = StringTest()
  errors += ChildTest()

  if errors:
    ErrOut.Log('IDLNode failed with %d errors.' % errors)
    return  -1
  return 0

if __name__ == '__main__':
  sys.exit(Main())

