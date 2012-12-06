#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
from idl_release import IDLRelease, IDLReleaseMap


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
# allow for look-ups.  IDLNode is derived from IDLRelease, so it is
# version aware.
#
class IDLNode(IDLRelease):

  # Set of object IDLNode types which have a name and belong in the namespace.
  NamedSet = set(['Enum', 'EnumItem', 'File', 'Function', 'Interface',
                  'Member', 'Param', 'Struct', 'Type', 'Typedef'])

  show_versions = False
  def __init__(self, cls, filename, lineno, pos, children=None):
    # Initialize with no starting or ending Version
    IDLRelease.__init__(self, None, None)

    self.cls = cls
    self.lineno = lineno
    self.pos = pos
    self.filename = filename
    self.filenode = None
    self.hashes = {}
    self.deps = {}
    self.errors = 0
    self.namespace = None
    self.typelist = None
    self.parent = None
    self.property_node = IDLPropertyNode()

    # A list of unique releases for this node
    self.releases = None

    # A map from any release, to the first unique release
    self.first_release = None

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
    ver = IDLRelease.__str__(self)
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
    if self.filenode:
      errcnt = self.filenode.GetProperty('ERRORS', 0)
      self.filenode.SetProperty('ERRORS', errcnt + 1)

  # Log a warning for this object
  def Warning(self, msg):
    WarnOut.LogLine(self.filename, self.lineno, 0, ' %s %s' %
                    (str(self), msg))

  def GetName(self):
    return self.GetProperty('NAME')

  def GetNameVersion(self):
    name = self.GetProperty('NAME', default='')
    ver = IDLRelease.__str__(self)
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
      out.write('%sComment\n' % tab)
      for line in self.GetName().split('\n'):
        out.write('%s  "%s"\n' % (tab, line))
    else:
      ver = IDLRelease.__str__(self)
      if self.releases:
        release_list = ': ' + ' '.join(self.releases)
      else:
        release_list = ': undefined'
      out.write('%s%s%s%s\n' % (tab, self, ver, release_list))
    if self.typelist:
      out.write('%s  Typelist: %s\n' % (tab, self.typelist.GetReleases()[0]))
    properties = self.property_node.GetPropertyList()
    if properties:
      out.write('%s  Properties\n' % tab)
      for p in properties:
        if is_comment and p == 'NAME':
          # Skip printing the name for comments, since we printed above already
          continue
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

  def GetType(self, release):
    if not self.typelist: return None
    return self.typelist.FindRelease(release)

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

  def GetDeps(self, release, visited=None):
    visited = visited or set()

    # If this release is not valid for this object, then done.
    if not self.IsRelease(release) or self.IsA('Comment', 'Copyright'):
      return set([])

    # If we have cached the info for this release, return the cached value
    deps = self.deps.get(release, None)
    if deps is not None:
      return deps

    # If we are already visited, then return
    if self in visited:
      return set([self])

    # Otherwise, build the dependency list
    visited |= set([self])
    deps = set([self])

    # Get child deps
    for child in self.GetChildren():
      deps |= child.GetDeps(release, visited)
      visited |= set(deps)

    # Get type deps
    typeref = self.GetType(release)
    if typeref:
      deps |= typeref.GetDeps(release, visited)

    self.deps[release] = deps
    return deps

  def GetVersion(self, release):
    filenode = self.GetProperty('FILE')
    if not filenode:
      return None
    return filenode.release_map.GetVersion(release)

  def GetUniqueReleases(self, releases):
    """Return the unique set of first releases corresponding to input

    Since we are returning the corresponding 'first' version for a
    release, we may return a release version prior to the one in the list."""
    my_min, my_max = self.GetMinMax(releases)
    if my_min > releases[-1] or my_max < releases[0]:
      return []

    out = set()
    for rel in releases:
      remapped = self.first_release[rel]
      if not remapped: continue
      out |= set([remapped])
    out = sorted(out)
    return out


  def GetRelease(self, version):
    filenode = self.GetProperty('FILE')
    if not filenode:
      return None
    return filenode.release_map.GetRelease(version)

  def _GetReleases(self, releases):
    if not self.releases:
      my_min, my_max = self.GetMinMax(releases)
      my_releases = [my_min]
      if my_max != releases[-1]:
        my_releases.append(my_max)
      my_releases = set(my_releases)
      for child in self.GetChildren():
        if child.IsA('Copyright', 'Comment', 'Label'):
          continue
        my_releases |= child.GetReleases(releases)
      self.releases = my_releases
    return self.releases


  def _GetReleaseList(self, releases, visited=None):
    visited = visited or set()
    if not self.releases:
      # If we are unversionable, then return first available release
      if self.IsA('Comment', 'Copyright', 'Label'):
        self.releases = []
        return self.releases

      # Generate the first and if deprecated within this subset, the
      # last release for this node
      my_min, my_max = self.GetMinMax(releases)

      if my_max != releases[-1]:
        my_releases = set([my_min, my_max])
      else:
        my_releases = set([my_min])

      # Break cycle if we reference ourselves
      if self in visited:
        return [my_min]

      visited |= set([self])

      # Files inherit all their releases from items in the file
      if self.IsA('AST', 'File'):
        my_releases = set()

      # Visit all children
      child_releases = set()

      # Exclude sibling results from parent visited set
      cur_visits = visited

      for child in self.children:
        child_releases |= set(child._GetReleaseList(releases, cur_visits))
        visited |= set(child_releases)

      # Visit my type
      type_releases = set()
      if self.typelist:
        type_list = self.typelist.GetReleases()
        for typenode in type_list:
          type_releases |= set(typenode._GetReleaseList(releases, cur_visits))

        type_release_list = sorted(type_releases)
        if my_min < type_release_list[0]:
          type_node = type_list[0]
          self.Error('requires %s in %s which is undefined at %s.' % (
              type_node, type_node.filename, my_min))

      for rel in child_releases | type_releases:
        if rel >= my_min and rel <= my_max:
          my_releases |= set([rel])

      self.releases = sorted(my_releases)
    return self.releases

  def GetReleaseList(self):
    return self.releases

  def BuildReleaseMap(self, releases):
    unique_list = self._GetReleaseList(releases)
    my_min, my_max = self.GetMinMax(releases)

    self.first_release = {}
    last_rel = None
    for rel in releases:
      if rel in unique_list:
        last_rel = rel
      self.first_release[rel] = last_rel
      if rel == my_max:
        last_rel = None

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
    self.release_map = IDLReleaseMap([('M13', 1.0)])


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

