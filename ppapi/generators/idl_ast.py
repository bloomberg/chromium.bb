#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Nodes for PPAPI IDL AST."""

from idl_namespace import IDLNamespace
from idl_node import IDLAttribute, IDLFile, IDLNode
from idl_option import GetOption
from idl_visitor import IDLVisitor
from idl_release import IDLReleaseList, IDLReleaseMap

#
# IDL Predefined types
#
BuiltIn = set(['int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t',
               'uint16_t', 'uint32_t', 'uint64_t', 'double_t', 'float_t',
               'handle_t', 'interface_t', 'char', 'mem_t', 'str_t', 'void'])


#
# IDLNamespaceLabelResolver
#
# Once the AST is build, we need to resolve the namespace and version
# information.
#
class IDLNamespaceLabelResolver(IDLVisitor):
  NamespaceSet = set(['AST', 'Callspec', 'Interface', 'Member', 'Struct'])
  #
  # When we arrive at a node we must assign it a namespace and if the
  # node is named, then place it in the appropriate namespace.
  #
  def Arrive(self, node, parent_namespace):
    # If we are entering a parent, clear the local Label\
    if node.IsA('File'): self.release_map = None

    # If this object is not a namespace aware object, use the parent's one
    if node.cls not in self.NamespaceSet:
      node.namespace = parent_namespace
    else:
    # otherwise create one.
      node.namespace = IDLNamespace(parent_namespace)
      node.namespace.name = node.GetName()

    # If this node is named, place it in its parent's namespace
    if parent_namespace and node.cls in IDLNode.NamedSet:
      # Set version min and max based on properties
      if self.release_map:
        vmin = node.GetProperty('version')
        vmax = node.GetProperty('deprecate')
        rmin = self.release_map.GetRelease(vmin)
        rmax = self.release_map.GetRelease(vmax)
        node.SetReleaseRange(rmin, rmax)
      parent_namespace.AddNode(node)

    # Pass this namespace to each child in case they inherit it
    return node.namespace

  #
  # As we return from a node, if the node is a LabelItem we pass back
  # the key=value pair representing the mapping of release to version.
  # If the node is a Label take the lists of mapping and generate a
  # version map which is assigned to the Labels parent as a property.
  #
  def Depart(self, node, data, childdata):
    if node.IsA('LabelItem'):
      return (node.GetName(), node.GetProperty('VALUE'))
    if node.IsA('Label') and node.GetName() == GetOption('label'):
      try:
        self.release_map = IDLReleaseMap(childdata)
        node.parent.release_map = self.release_map
      except Exception as err:
        node.Error('Unable to build release map: %s' % str(err))
    return None


class IDLFileTypeResolver(IDLVisitor):
  def VisitFilter(self, node, data):
    return not node.IsA('Comment', 'Copyright')

  def Arrive(self, node, filenode):
    # Track the file node to update errors
    if node.IsA('File'):
      node.SetProperty('FILE', node)

    # If this node has a TYPEREF, resolve it to a version list
    typeref = node.property_node.GetPropertyLocal('TYPEREF')
    if typeref:
      node.typelist = node.parent.namespace.FindList(typeref)
      if not node.typelist:
        node.Error('Could not resolve %s.' % typeref)
    else:
      node.typelist = None
    return filenode


#
# IDLAst
#
# A specialized version of the IDLNode for containing the whole of the
# AST.  The specialized BuildTree function pulls the per file namespaces
# into the global AST namespace and checks for collisions.
#
class IDLAst(IDLNode):
  def __init__(self, children):
    objs = []

    builtin = None
    extranodes = []
    for filenode in children:
      if filenode.GetProperty('NAME') == 'pp_stdint.idl':
        builtin = filenode
        break

    IDLNode.__init__(self, 'AST', 'BuiltIn', 1, 0, extranodes + children)
    self.Resolve()

  def Resolve(self):
    self.namespace = IDLNamespace(None)
    self.namespace.name = 'AST'
    IDLNamespaceLabelResolver().Visit(self, self.namespace)
    IDLFileTypeResolver().Visit(self, None)

    # Build an ordered list of all releases
    self.releases = set()
    for filenode in self.GetListOf('File'):
      self.releases |= set(filenode.release_map.GetReleases())
    self.releases = sorted(self.releases)

  def SetTypeInfo(self, name, properties):
    node = self.namespace[name]
    for prop in properties:
      node.properties[prop] = properties[prop]


