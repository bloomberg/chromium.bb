#!/usr/bin/python
#
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Nodes for PPAPI IDL AST """

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

import re
import sys
import hashlib

from idl_log import ErrOut, InfoOut, WarnOut

#
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

# Set of object IDLNode types which have a name and belong in the namespace.
NamedSet = set(['Enum', 'EnumItem', 'Function', 'Interface', 'Member', 'Param',
               'Struct', 'Type', 'Typedef'])

#
# IDLNode
#
# A node in the AST.  The Node is composed of children, implemented as a
# dictionary of lists of child types and a dictionary of local properties,
# a.k.a. ExtendedAttributes.
#
class IDLNode(object):
  # Regular expression to parse property keys in a string such that a string
  #  "My string #NAME#" will find the key "NAME".
  regex_var = re.compile("(?P<src>[^\\$]+)|(?P<key>\\$\\w+\\$)")

  def __init__(self, cls, name, filename, lineno, pos, children):
    self.cls = cls
    self.name = name
    self.lineno = lineno
    self.pos = pos
    self.filename = filename

    # Dictionary of type to in order child list
    self.children = {}

    # Dictionary of child name to child object
    self.namespace = {}

    # Dictionary of properties (ExtAttributes)
    self.properties = { 'NAME' : name }

    self.hash = None
    self.typeref = None
    self.parent = None
    self.childlist = children

    if children:
      for child in children:
        #
        # Copy children into local dictionary such that children of the
        # same class are added in order to the per key list.  All
        # ExtAttributes are filtered and applied to a property dictionary
        # instead of becoming children of IDLNode
        #
        if child.cls == 'ExtAttribute':
          self.properties[child.name] =  child.value
        else:
          if child.cls in self.children:
            self.children[child.cls].append(child)
          else:
            self.children[child.cls] = [child]

  # Return a string representation of this node
  def __str__(self):
    return "%s(%s)" % (self.cls, self.name)

  # Log an error for this object
  def Error(self, msg):
    ErrOut.LogLine(self.filename, self.lineno, 0, " %s %s" %
                   (str(self), msg))

  # Log a warning for this object
  def Warning(self, msg):
    WarnOut.LogLine(self.filename, self.lineno, 0, " %s %s" %
                    (str(self), msg))

  # Get a list of objects for this key
  def GetListOf(self, key):
    return self.children.get(key, [])

  def GetOneOf(self, key):
    children = self.children.get(key, None)
    if children:
      assert(len(children) == 1)
      return children[0]
    return None

  # Get a list of all objects
  def Children(self):
    out = []
    for key in sorted(self.children.keys()):
      out.extend(self.children[key])
    return out

  # Dump this object and its children
  def Dump(self, depth=0, comments=False, out=sys.stdout):
    if self.cls == 'Comment' or self.cls == 'Copyright':
      is_comment = True
    else:
      is_comment = False

    # Skip this node if it's a comment, and we are not printing comments
    if not comments and is_comment: return

    tab = ""
    for t in range(depth):
      tab += '    '

    if is_comment:
      for line in self.name.split('\n'):
        out.write("%s%s\n" % (tab, line))
    else:
      out.write("%s%s\n" % (tab, self))

    if self.properties:
      out.write("%s  Properties\n" % tab)
      for p in self.properties:
        out.write("%s    %s : %s\n" % (tab, p, self.properties[p]))

    for cls in sorted(self.children.keys()):
      # Skip comments
      if (cls == 'Comment' or cls == 'Copyright') and not comments: continue

      out.write("%s  %ss\n" % (tab, cls))
      for c in self.children[cls]:
        c.Dump(depth + 1, comments=comments, out=out)

  # Link the parents and add the object to the parent's namespace
  def BuildTree(self, parent):
    self.parent = parent
    for child in self.Children():
      child.BuildTree(self)
      name = child.name

      # Add this child to the local namespace if it's a 'named' type
      if child.cls in NamedSet:
        if name in self.namespace:
          other = self.namespace[name]
          child.Error('Attempting to add %s to namespace of %s when already '
                      'declared in %s' % (name, str(self), str(other)))
        self.namespace[name] = child

  def Resolve(self):
    errs = 0
    typename = self.properties.get('TYPEREF', None)

    if typename:
      if typename in self.namespace:
        self.Error('Type collision in local namespace')
        errs += 1

      typeinfo = self.parent.Find(typename)
      if not typeinfo:
        self.Error('Unable to resolve typename %s.' % typename)
        errs += 1
        sys.exit(-1)
      self.typeinfo = typeinfo
    else:
      self.typeinfo = None

    for child in self.Children():
      errs += child.Resolve()

    return errs

  def Find(self, name):
    if name in self.namespace: return self.namespace[name]
    if self.parent: return self.parent.Find(name)
    return None

  def Hash(self):
    hash = hashlib.sha1()
    for key, val in self.properties.iteritems():
      hash.update('%s=%s' % (key, str(val)))
    for child in self.Children():
      hash.update(child.Hash())
    if self.typeref: hash.update(self.typeref.hash)
    self.hash = hash.hexdigest()
    return self.hash

  def GetProperty(self, name):
    return self.properties.get(name, None)

  # Recursively expands text keys in the form of $KEY$ with the value
  # of the property of the same name.  Since this is done recursively
  # one property can be defined in tems of another.
  def Replace(self, text):
    itr = IDLNode.regex_var.finditer(text)
    out = ""
    for m in itr:
      (min,max) = m.span()
      if m.lastgroup == "src":
        out += text[min:max]
      if m.lastgroup == "key":
        key = text[min+1:max-1]
        val = self.properties.get(key, None)
        if not val:
          self.Error("No property '%s'" % key)
        out += self.Replace(str(val))
    return out
#
# IDL Predefined types
#
BuiltIn = set(['int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t',
              'uint32_t', 'uint64_t', 'double_t', 'float_t', 'handle_t',
              'mem_t', 'str_t', 'void', 'enum', 'struct', 'bool'])

#
# IDLAst
#
# A specialized version of the IDLNode for containing the whole of the
# AST.  The specialized BuildTree function pulls the per file namespaces
# into the global AST namespace and checks for collisions.
#
class IDLAst(IDLNode):
  def __init__(self, children):
    objs = [IDLNode('Type', name, 'BuiltIn', 1, 0, None) for name in BuiltIn]
    IDLNode.__init__(self, 'AST', 'PPAPI', 'BuiltIn', 1, 0, objs + children)

  def BuildTree(self, parent):
    IDLNode.BuildTree(self, parent)
    for fileobj in self.GetListOf('File'):
      for name, val in fileobj.namespace.iteritems():
        if name in self.namespace:
          other = self.namespace[name]
          val.Error('Attempting to add %s to namespace of %s when already '
                    'declared in %s' % (name, str(self), str(other)))
        self.namespace[name] = val

