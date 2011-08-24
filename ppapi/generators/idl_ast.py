#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Nodes for PPAPI IDL AST."""

from idl_namespace import IDLNamespace, IDLVersionMap
from idl_node import IDLAttribute, IDLFile, IDLNode
from idl_option import GetOption
from idl_visitor import IDLVisitor

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
    # Set version min and max based on properties
    vmin = node.GetProperty('version')
    vmax = node.GetProperty('deprecate')
    node.SetVersionRange(vmin, vmax)

    # If this object is not a namespace aware object, use the parent's one
    if node.cls not in self.NamespaceSet:
      node.namespace = parent_namespace
    else:
    # otherwise create one.
      node.namespace = IDLNamespace(parent_namespace)
      node.namespace.name = node.GetName()

    # If this node is named, place it in its parent's namespace
    if parent_namespace and node.cls in IDLNode.NamedSet:
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
      vmap = IDLVersionMap()
      for release, version in childdata:
        vmap.AddReleaseVersionMapping(release, float(version))
      node.parent.SetProperty("LABEL", vmap)
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


class IDLReleaseResolver(IDLVisitor):
  def VisitFilter(self, node, data):
    return node.IsA('AST','File', 'Label')

  def Depart(self, node, data, childdata):
    if node.IsA('Label'):
      return set([child.name for child in GetListOf('LabelItem')])
    return childdata

class IDLVersionMapDefault(IDLVersionMap):
  def GetRelease(self, version):
    return 'M13'

  def GetVersion(self, release):
    return float(0.0)

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
    self.SetProperty('LABEL', IDLVersionMapDefault())
    self.Resolve()

  def Resolve(self):
    self.namespace = IDLNamespace(None)
    self.namespace.name = 'AST'
    IDLNamespaceLabelResolver().Visit(self, self.namespace)
    IDLFileTypeResolver().Visit(self, None)

    # Build an ordered list of all releases
    self.releases = set()
    for filenode in self.GetListOf('File'):
      vmap = filenode.GetProperty('LABEL')
      self.releases |= set(vmap.releases)
    self.releases = sorted(self.releases)

  def SetTypeInfo(self, name, properties):
    node = self.namespace[name]
    for prop in properties:
      node.properties[prop] = properties[prop]


